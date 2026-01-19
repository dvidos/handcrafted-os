#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


typedef uint32_t entity_t;

// ---------------------------------
// Components
// ---------------------------------
typedef struct position {
    entity_t entity;
    int x, y;
    struct position *next;
} position_t;

typedef struct size {
    entity_t entity;
    int width, height;
    struct size *next;
} size_t;

typedef struct clickable {
    entity_t entity;
    bool pressed;
    struct clickable *next;
} clickable_t;

// ---------------------------------
// ECS world
// ---------------------------------
typedef struct {
    entity_t next_entity;
    position_t *positions;
    size_t *sizes;
    clickable_t *clickables;
} ecs_world_t;

// ---------------------------------
// Entity management
// ---------------------------------
static inline entity_t ecs_create_entity(ecs_world_t *world) {
    return world->next_entity++;
}

// ---------------------------------
// Component adders
// ---------------------------------
static inline void ecs_add_position(ecs_world_t *world, entity_t e, int x, int y) {
    position_t *comp = malloc(sizeof(position_t));
    comp->entity = e;
    comp->x = x;
    comp->y = y;
    comp->next = world->positions;
    world->positions = comp;
}

static inline void ecs_add_size(ecs_world_t *world, entity_t e, int w, int h) {
    size_t *comp = malloc(sizeof(size_t));
    comp->entity = e;
    comp->width = w;
    comp->height = h;
    comp->next = world->sizes;
    world->sizes = comp;
}

static inline void ecs_add_clickable(ecs_world_t *world, entity_t e, bool pressed) {
    clickable_t *comp = malloc(sizeof(clickable_t));
    comp->entity = e;
    comp->pressed = pressed;
    comp->next = world->clickables;
    world->clickables = comp;
}

// ---------------------------------
// Helper to find a component by entity
// ---------------------------------
static inline position_t* ecs_get_position(ecs_world_t *world, entity_t e) {
    for (position_t *p = world->positions; p; p = p->next)
        if (p->entity == e) return p;
    return NULL;
}

static inline size_t* ecs_get_size(ecs_world_t *world, entity_t e) {
    for (size_t *s = world->sizes; s; s = s->next)
        if (s->entity == e) return s;
    return NULL;
}

static inline clickable_t* ecs_get_clickable(ecs_world_t *world, entity_t e) {
    for (clickable_t *c = world->clickables; c; c = c->next)
        if (c->entity == e) return c;
    return NULL;
}

// ---------------------------------
// Example system: click detection
// ---------------------------------
static inline void ecs_process_clicks(ecs_world_t *world, int mouse_x, int mouse_y) {
    for (clickable_t *c = world->clickables; c; c = c->next) {
        position_t *p = ecs_get_position(world, c->entity);
        size_t *s = ecs_get_size(world, c->entity);
        if (p && s) {
            if (mouse_x >= p->x && mouse_x <= p->x + s->width &&
                mouse_y >= p->y && mouse_y <= p->y + s->height) {
                c->pressed = true;
                printf("Entity %d clicked!\n", c->entity);
            }
        }
    }
}

