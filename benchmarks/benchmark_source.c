#include "mcl/wire.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MCL_BENCH_FILE_CAPACITY 16384u
#define MCL_BENCH_MAX_FIELDS 16u
#define MCL_BENCH_MAX_KEY 32u

typedef struct {
    char key[MCL_BENCH_MAX_KEY];
    int64_t value;
} bench_field_t;

typedef struct {
    bench_field_t fields[MCL_BENCH_MAX_FIELDS];
    size_t count;
} bench_event_t;

typedef struct {
    const char *name;
    uint8_t protobuf_number;
    uint8_t protobuf_signed;
} field_info_t;

static const field_info_t field_info[] = {
    {"type",1u,0u},{"priority",2u,0u},{"source_ref",3u,0u},
    {"machine_class",4u,0u},{"capability_tag",5u,0u},{"ttl",6u,0u},
    {"hazard_class",7u,0u},{"severity",8u,0u},{"confidence",9u,0u},
    {"x",10u,1u},{"y",11u,1u},{"z",12u,1u},{"radius",13u,0u},
    {"request_class",14u,0u},{"target_ref",15u,0u},
    {"authority_class",16u,0u},{"jurisdiction",17u,0u},
    {"credential_ref",18u,0u},{"validity",19u,0u},
    {"affected_capability",20u,0u},{"health",21u,0u},
    {"transport_id",23u,0u},{"profile_id",24u,0u},{"endpoint_token",25u,0u}
};

static void bench_require(int condition, const char *expression)
{
    if (condition == 0) {
        fprintf(stderr, "benchmark validation failed: %s\n", expression);
        exit(EXIT_FAILURE);
    }
}

#define BENCH_REQUIRE(condition) bench_require((condition), #condition)

static const field_info_t *lookup_field(const char *name)
{
    size_t i;
    for (i=0u;i<sizeof(field_info)/sizeof(field_info[0]);++i) {
        if (strcmp(name,field_info[i].name)==0) return &field_info[i];
    }
    return NULL;
}

static const bench_field_t *get_field(const bench_event_t *event, const char *name)
{
    size_t i;
    for (i=0u;i<event->count;++i) {
        if (strcmp(event->fields[i].key,name)==0) return &event->fields[i];
    }
    return NULL;
}

static int64_t require_value(const bench_event_t *event, const char *name)
{
    const bench_field_t *field=get_field(event,name);
    BENCH_REQUIRE(field!=NULL);
    return field->value;
}

static const char *skip_ws(const char *p, const char *end)
{
    while (p<end && isspace((unsigned char)*p)!=0) ++p;
    return p;
}

static const char *parse_string(const char *p, const char *end, char *out, size_t out_size)
{
    size_t n=0u;
    p=skip_ws(p,end);
    if (p>=end || *p!='\"') return NULL;
    ++p;
    while (p<end && *p!='\"') {
        if (*p=='\\' || n+1u>=out_size) return NULL;
        out[n++]=*p++;
    }
    if (p>=end || *p!='\"') return NULL;
    out[n]='\0';
    return p+1;
}

static const char *parse_integer(const char *p, const char *end, int64_t *value)
{
    int negative=0;
    uint64_t magnitude=0u;
    p=skip_ws(p,end);
    if (p<end && *p=='-') { negative=1; ++p; }
    if (p>=end || *p<'0' || *p>'9') return NULL;
    while (p<end && *p>='0' && *p<='9') {
        magnitude=magnitude*10u+(uint64_t)(*p-'0');
        ++p;
    }
    if (negative!=0) {
        if (magnitude>UINT64_C(9223372036854775808)) return NULL;
        if (magnitude==UINT64_C(9223372036854775808)) *value=INT64_MIN;
        else *value=-(int64_t)magnitude;
    } else {
        if (magnitude>(uint64_t)INT64_MAX) return NULL;
        *value=(int64_t)magnitude;
    }
    return p;
}

static const char *parse_event(const char *p, const char *end, bench_event_t *event)
{
    p=skip_ws(p,end);
    if (p>=end || *p!='{') return NULL;
    ++p;
    event->count=0u;
    for (;;) {
        bench_field_t *field;
        p=skip_ws(p,end);
        if (p<end && *p=='}') return p+1;
        if (event->count>=MCL_BENCH_MAX_FIELDS) return NULL;
        field=&event->fields[event->count];
        p=parse_string(p,end,field->key,sizeof(field->key));
        if (p==NULL) return NULL;
        if (lookup_field(field->key)==NULL) return NULL;
        p=skip_ws(p,end);
        if (p>=end || *p!=':') return NULL;
        p=parse_integer(p+1,end,&field->value);
        if (p==NULL) return NULL;
        ++event->count;
        p=skip_ws(p,end);
        if (p>=end) return NULL;
        if (*p==',') { ++p; continue; }
        if (*p=='}') return p+1;
        return NULL;
    }
}

static size_t decimal_size(int64_t value)
{
    uint64_t magnitude;
    size_t size=1u;
    if (value<0) { magnitude=(uint64_t)(-(value+1))+1u; ++size; }
    else magnitude=(uint64_t)value;
    while (magnitude>=10u) { magnitude/=10u; ++size; }
    return size;
}

static size_t cbor_head_size(uint64_t value)
{
    if (value<24u) return 1u;
    if (value<=UINT8_MAX) return 2u;
    if (value<=UINT16_MAX) return 3u;
    if (value<=UINT32_MAX) return 5u;
    return 9u;
}

static size_t cbor_int_size(int64_t value)
{
    return cbor_head_size(value>=0 ? (uint64_t)value : (uint64_t)(-(value+1)));
}

static size_t cbor_text_size(const char *text)
{
    const size_t length=strlen(text);
    return cbor_head_size((uint64_t)length)+length;
}

static size_t varint_size(uint64_t value)
{
    size_t size=1u;
    while (value>=0x80u) { value>>=7; ++size; }
    return size;
}

static uint64_t zigzag32(int64_t value)
{
    const int32_t narrowed=(int32_t)value;
    if (narrowed<0) {
        const uint32_t magnitude=(uint32_t)(-(int64_t)narrowed);
        return (uint64_t)((magnitude<<1)-1u);
    }
    return (uint64_t)((uint32_t)narrowed<<1);
}

static size_t json_size(const bench_event_t *event)
{
    size_t size=2u;
    size_t i;
    for (i=0u;i<event->count;++i) {
        if (i!=0u) ++size;
        size+=2u+strlen(event->fields[i].key)+1u+decimal_size(event->fields[i].value);
    }
    return size;
}

static size_t cbor_size(const bench_event_t *event, int integer_keys)
{
    size_t size=cbor_head_size(event->count);
    size_t i;
    for (i=0u;i<event->count;++i) {
        const field_info_t *info=lookup_field(event->fields[i].key);
        BENCH_REQUIRE(info!=NULL);
        size+=integer_keys!=0 ? cbor_head_size(info->protobuf_number) : cbor_text_size(info->name);
        size+=cbor_int_size(event->fields[i].value);
    }
    return size;
}

static size_t protobuf_size(const bench_event_t *event)
{
    size_t size=0u;
    size_t i;
    for (i=0u;i<event->count;++i) {
        const field_info_t *info=lookup_field(event->fields[i].key);
        uint64_t value;
        BENCH_REQUIRE(info!=NULL);
        if (event->fields[i].value==0) continue;
        size+=varint_size((uint64_t)((uint32_t)info->protobuf_number<<3));
        value=info->protobuf_signed!=0u ? zigzag32(event->fields[i].value) : (uint64_t)event->fields[i].value;
        size+=varint_size(value);
    }
    return size;
}

static void populate_object(const bench_event_t *event, mcl_wire_tier0_t *object)
{
    const int64_t type=require_value(event,"type");
    memset(object,0,sizeof(*object));
    object->kind=(mcl_wire_kind_t)type;
    object->priority=(uint8_t)require_value(event,"priority");
    object->source_ref=(uint32_t)require_value(event,"source_ref");
    switch (object->kind) {
    case MCL_WIRE_KIND_PRESENCE:
        object->body.presence.machine_class=(uint8_t)require_value(event,"machine_class");
        object->body.presence.capability_tag=(uint32_t)require_value(event,"capability_tag");
        object->body.presence.ttl=(uint8_t)require_value(event,"ttl");
        break;
    case MCL_WIRE_KIND_HAZARD:
        object->body.hazard.hazard_class=(uint8_t)require_value(event,"hazard_class");
        object->body.hazard.severity=(uint8_t)require_value(event,"severity");
        object->body.hazard.confidence=(uint8_t)require_value(event,"confidence");
        object->body.hazard.x=(int16_t)require_value(event,"x");
        object->body.hazard.y=(int16_t)require_value(event,"y");
        object->body.hazard.z=(int16_t)require_value(event,"z");
        object->body.hazard.radius=(uint16_t)require_value(event,"radius");
        object->body.hazard.ttl=(uint8_t)require_value(event,"ttl");
        break;
    case MCL_WIRE_KIND_REQUEST:
        object->body.request.request_class=(uint8_t)require_value(event,"request_class");
        object->body.request.target_ref=(uint32_t)require_value(event,"target_ref");
        object->body.request.x=(int16_t)require_value(event,"x");
        object->body.request.y=(int16_t)require_value(event,"y");
        object->body.request.radius=(uint16_t)require_value(event,"radius");
        object->body.request.ttl=(uint8_t)require_value(event,"ttl");
        break;
    case MCL_WIRE_KIND_AUTHORITY_CLAIM:
        object->body.authority_claim.authority_class=(uint8_t)require_value(event,"authority_class");
        object->body.authority_claim.jurisdiction=(uint16_t)require_value(event,"jurisdiction");
        object->body.authority_claim.credential_ref=(uint32_t)require_value(event,"credential_ref");
        object->body.authority_claim.validity=(uint8_t)require_value(event,"validity");
        break;
    case MCL_WIRE_KIND_DEGRADED_STATE:
        object->body.degraded_state.affected_capability=(uint8_t)require_value(event,"affected_capability");
        object->body.degraded_state.health=(uint8_t)require_value(event,"health");
        object->body.degraded_state.severity=(uint8_t)require_value(event,"severity");
        object->body.degraded_state.ttl=(uint8_t)require_value(event,"ttl");
        break;
    case MCL_WIRE_KIND_TRANSPORT_OFFER:
        /*
         * The derived case file predates migration_ref and carries no value for
         * it. Set a fixed one rather than leaving it indeterminate: this
         * benchmark round-trips each object and compares the structs, so an
         * uninitialised field would make the comparison depend on stack
         * residue. The constant measures size, and says nothing about protocol
         * validity.
         */
        object->body.transport_offer.migration_ref=UINT32_C(1);
        object->body.transport_offer.transport_id=(uint8_t)require_value(event,"transport_id");
        object->body.transport_offer.profile_id=(uint8_t)require_value(event,"profile_id");
        object->body.transport_offer.endpoint_token=(uint32_t)require_value(event,"endpoint_token");
        object->body.transport_offer.validity=(uint8_t)require_value(event,"validity");
        break;
    default:
        BENCH_REQUIRE(0);
    }
}

int main(int argc, char **argv)
{
    const char *path=argc>1 ? argv[1] : "benchmarks/cases-v0.1.json";
    char buffer[MCL_BENCH_FILE_CAPACITY];
    FILE *file=fopen(path,"rb");
    size_t length;
    const char *events;
    const char *p;
    const char *end;
    uint64_t json_total=0u,cbor_string_total=0u,cbor_integer_total=0u;
    uint64_t protobuf_total=0u,fixed_total=0u,context_total=0u;
    size_t case_count=0u;

    if (file==NULL) { perror(path); return 1; }
    length=fread(buffer,1u,sizeof(buffer)-1u,file);
    if (ferror(file)!=0 || fclose(file)!=0 || length==sizeof(buffer)-1u) return 2;
    buffer[length]='\0';
    end=buffer+length;
    events=strstr(buffer,"\"events\":[");
    if (events==NULL) return 3;
    p=events+strlen("\"events\":[");

    for (;;) {
        bench_event_t event;
        mcl_wire_tier0_t object,decoded;
        uint8_t encoded[MCL_WIRE_TIER0_MAX_SIZE];
        size_t written=0u,consumed=0u,fixed_size;
        p=skip_ws(p,end);
        if (p>=end) return 4;
        if (*p==']') break;
        p=parse_event(p,end,&event);
        if (p==NULL) return 5;
        populate_object(&event,&object);
        fixed_size=mcl_wire_tier0_encoded_size(object.kind);
        BENCH_REQUIRE(mcl_wire_tier0_encode(&object,encoded,sizeof(encoded),&written)==MCL_WIRE_OK);
        BENCH_REQUIRE(written==fixed_size);
        BENCH_REQUIRE(mcl_wire_tier0_decode(encoded,written,&decoded,&consumed)==MCL_WIRE_OK);
        BENCH_REQUIRE(consumed==written);
        BENCH_REQUIRE(memcmp(&object,&decoded,sizeof(object))==0);
        json_total+=json_size(&event);
        cbor_string_total+=cbor_size(&event,0);
        cbor_integer_total+=cbor_size(&event,1);
        protobuf_total+=protobuf_size(&event);
        fixed_total+=fixed_size;
        context_total+=fixed_size-3u;
        ++case_count;
        p=skip_ws(p,end);
        if (p<end && *p==',') ++p;
    }

    BENCH_REQUIRE(case_count==41u);
    BENCH_REQUIRE(json_total==UINT64_C(4999));
    BENCH_REQUIRE(cbor_string_total==UINT64_C(3585));
    BENCH_REQUIRE(cbor_integer_total==UINT64_C(1085));
    BENCH_REQUIRE(protobuf_total==UINT64_C(922));
        /*
     * 602 -> 618 and 479 -> 495 because TRANSPORT_OFFER grew from 13 to 17
     * bytes when migration_ref was added, across the four TRANSPORT_OFFER
     * events in this corpus. The retained v0.1 result file is NOT edited; a
     * v0.2 result records the new figures alongside it.
     */
    BENCH_REQUIRE(fixed_total==UINT64_C(618));
    BENCH_REQUIRE(context_total==UINT64_C(495));

    printf("cases: %zu\n",case_count);
    printf("compact JSON mean: %.3f B\n",(double)json_total/(double)case_count);
    printf("CBOR string-key mean: %.3f B\n",(double)cbor_string_total/(double)case_count);
    printf("CBOR integer-key mean: %.3f B\n",(double)cbor_integer_total/(double)case_count);
    printf("typed proto3-equivalent mean: %.3f B\n",(double)protobuf_total/(double)case_count);
    printf("MCL fixed Tier-0 mean: %.3f B\n",(double)fixed_total/(double)case_count);
    printf("historical established-context candidate mean: %.3f B\n",(double)context_total/(double)case_count);
    return 0;
}
