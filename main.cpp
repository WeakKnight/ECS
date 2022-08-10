#include <stdint.h>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <typeinfo>

typedef uint32_t Entity;

struct Position
{
    float x;
    float y;
    float z;
};

static std::atomic<uint32_t> sEntityIndex = 0;
static std::unordered_map<const char*, std::vector<void*>> sComponents;
static std::unordered_map<const char*, std::unordered_map<Entity, size_t>> sLookupMap;

Entity entity_create()
{
    return sEntityIndex++;
}

template<typename T>
void entity_add_component(Entity entity)
{

}

int main()
{
    Entity entity = entity_create();

    sComponents["Position"] = {};

    Position positionCom = {};

    return 0;
}