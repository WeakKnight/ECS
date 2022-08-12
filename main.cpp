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

int main()
{
	for (int i = 0; i < 10; i++)
	{
		Entity entity = Entity::Create();

		entity.AddComponent<Position>(i * 1.0f, i * 1.0f);

		if (i % 2 == 0)
		{
			entity.AddComponent<Velocity>(i * 1.0f, i * 1.0f);
		}
	}

	Entity::ForEach<Position, Velocity>([](Position* position, Velocity* velocity) {
		position->x += velocity->dx;
		position->y += velocity->dy;
		printf("[x: %f, y: %f]\n", position->x, position->y);
	});

	Entity::Release();

	return 0;
}