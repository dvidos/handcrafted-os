#include <stdio.h>
#include "ecs.h"


int main(void) {
    // Initialize ECS world
    ecs_world_t world = {0};

    // Create some entities
    entity_t e1 = ecs_create_entity(&world);
    entity_t e2 = ecs_create_entity(&world);

    // Add components
    ecs_add_position(&world, e1, 10, 10);
    ecs_add_size(&world, e1, 50, 50);
    ecs_add_clickable(&world, e1, false);

    ecs_add_position(&world, e2, 100, 100);
    ecs_add_size(&world, e2, 30, 30);
    ecs_add_clickable(&world, e2, false);

    // Simulate a mouse click at (20, 20)
    printf("Click at (20,20):\n");
    ecs_process_clicks(&world, 20, 20);

    // Simulate a mouse click at (105, 110)
    printf("Click at (105,110):\n");
    ecs_process_clicks(&world, 105, 110);

    // Simulate a click at (0,0) - should hit nothing
    printf("Click at (0,0):\n");
    ecs_process_clicks(&world, 0, 0);

    // Check which entities were pressed
    for (entity_t e = 0; e < world.next_entity; e++) {
        clickable_t *c = ecs_get_clickable(&world, e);
        if (c && c->pressed) {
            printf("Entity %d was pressed.\n", e);
        }
    }

    return 0;
}
