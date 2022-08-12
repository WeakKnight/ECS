#include "ECS.h"

struct Position
{
	float x;
	float y;
};

struct Velocity
{
	float dx;
	float dy;
};

int main()
{
	for (int i = 0; i < 10; i++)
	{
		Entity entity = Entity::Create();

		Position* position = entity.AddComponent<Position>();
		position->x = i * 1.0f;
		position->y = i * 1.0f;

		if (i % 2 == 0)
		{
			Velocity* velocity = entity.AddComponent<Velocity>();
			velocity->dx = i * 1.0f;
			velocity->dy = i * 1.0f;
		}
	}

	Entity::ForEach<Position, Velocity>([](Position* position, Velocity* velocity) {
		position->x += velocity->dx;
		position->y += velocity->dy;
	});

	Entity::Release();

	return 0;
}