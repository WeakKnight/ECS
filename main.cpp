#include "ECS.h"

struct Position
{
	Position(float px, float py)
	{
		x = px;
		y = py;
	}

	float x;
	float y;
};

struct Velocity
{
	Velocity(float pdx, float pdy)
	{
		dx = pdx;
		dy = pdy;
	}

	float dx;
	float dy;
};

struct Movable
{
};

typedef ECS::Entity Entity;

int main()
{
	ECS::Init();

	for (int i = 0; i < 100; i++)
	{
		Entity entity = Entity::Create();

		entity.AddComponent<Position>(i * 1.0f, i * 1.0f);

		if (i % 2 == 0)
		{
			entity.AddComponent<Velocity>(i * 1.0f, i * 1.0f);
			if (i % 3 == 0)
			{
				auto movable = entity.AddComponent<Movable>();
				assert(entity == Entity::FromComponent(movable));
			}
		}
	}

	Entity::ForEach<Position, Velocity, Movable>([](Position* position, Velocity* velocity, Movable*) {
		position->x += velocity->dx;
		position->y += velocity->dy;
		printf("[x: %f, y: %f]\n", position->x, position->y);
	});

	ECS::Release();

	return 0;
}