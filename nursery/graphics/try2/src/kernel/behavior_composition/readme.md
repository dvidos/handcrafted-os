# ECS-Style UI Design — Summary and Examples

This document summarizes how an ECS-like (Entity–Component–System) model can be applied to a UI system, especially in C, and contrasts it with traditional widget class hierarchies.

---

## 1. Core Idea

### Entity
- Just an ID (e.g. integer).
- Represents a UI object (view, menu item, cursor, etc).
- Has no behavior by itself.

### Component
- Plain data attached to an entity.
- Describes *what the entity is* or *what state it has*.

Examples:
- Bounds / Rect
- Text
- Clickable
- Focusable
- HoverState
- Disabled
- MenuItem
- ScrollOffset

### System
- A function that iterates over entities with specific components.
- Implements behavior and state transitions.

Examples:
- LayoutSystem
- HitTestSystem
- InputDispatchSystem
- FocusSystem
- AnimationSystem
- RenderSystem
- MenuSystem

---

## 2. ECS vs Widget Hierarchies

### Widget Hierarchy Model

- Behavior is tied to types (Button, TextBox, ListBox).
- Extending behavior usually means subclassing.
- Leads to deep inheritance trees and combinatorial explosion.

Example problem:
- FocusableButton
- DisabledFocusableButton
- FocusableButtonWithTooltip
- etc.

---

### ECS / Composition Model

- Behavior is built by attaching components.
- Systems interpret combinations of components.
- New features are added by new components + systems.

Example:
- A "button" is just an entity with:
  - Bounds
  - Text
  - Clickable
  - Focusable
  - StateStyling

No special Button type is required.

---

## 3. Why ECS Fits C Well

- C naturally supports:
  - Structs (components)
  - Arrays (component storage)
  - Function pointers (systems / callbacks)

- No need to fake inheritance:
  - No base structs
  - No manual casting
  - No fragile layout assumptions

- Data-oriented:
  - Cache-friendly iteration
  - Predictable control flow

---

## 4. Example: Traditional vs ECS-Style

### Traditional Widget Style (C-ish OO)

```c
typedef struct {
    view_t base;
    const char *text;
    bool pressed;
} button_t;
```

Behavior is tied to the button type.

### ECS-Style Components

```c
struct Rect { int x, y, w, h; };
struct Text { const char *str; };
struct Clickable { bool pressed; };
struct Focusable { bool has_focus; };
```

Entity 42 might have all of the above components.

### System Example

```c
for each entity with Rect + Clickable:
    if mouse_inside(rect) and mouse_down:
        clickable.pressed = true;
```

Rendering is done by another system:

```c
for each entity with Rect + Text:
    draw_text(rect, text);
```

---

## 5. Minimal UI Behavior Set (Practical)

### Geometry & Layout

- Bounds / Rect
- LayoutContainer (vertical, horizontal, absolute)

### Rendering

- BackgroundFill
- Border
- TextRender
- Input
- HitTest
- Clickable
- Focusable
- KeyHandler

### State

- HoverState
- PressedState
- Enabled / Disabled

### Containers

- ChildrenList
- EventPropagation

### Menus

- MenuContainer
- MenuItemSelectable

Popup menu = surface + MenuContainer + MenuItemSelectable children

---

## 6. Mid-Level Mature UI Behavior Set

### Interaction & Navigation

- TabNavigation
- DefaultAction (Enter)
- CancelAction (Esc)
- ScrollInput

### Visual Feedback

- ThemeStyle
- StateStyling (hover/focus/disabled)
- Animation
- Clipping

### Layout

- FlexLayout / GridLayout
- IntrinsicSize
- ScrollContainer

### Text Editing

- TextSelectable
- TextCursor
- TextEdit

### Advanced Input

- DragSource
- DropTarget
- ResizeHandles

### Window & Menu System

- MenuAnchor
- SubmenuSpawner
- ModalBlocker
- DialogOwner

### Accessibility

- AccessibleRole
- AccessibleLabel

## 7. Behavior Composition Without Full ECS

A practical intermediate approach:

### View with Behavior Chain

```c
struct behavior {
    bool (*on_event)(view_t *, event_t *);
    void (*on_paint)(view_t *, surface_t *);
    struct behavior *next;
};
```

Views store:

- geometry
- state
- behavior chain

Behaviors act like middleware:

- hover behavior
- focus behavior
- button behavior

This provides composition without full ECS storage.

---

## 8. Systems and Execution Order

Typical UI frame:

1. Input collection
1. Hit testing
1. Focus updates
1. Behavior handling
1. Layout pass
1. Animation update
1. Rendering

Ordering becomes the main architectural rule instead of call stacks.

---

## 9. Tradeoffs

### Advantages

- Strong composition model
- No inheritance hierarchies
- Easier to add cross-cutting features
- Good fit for C

### Costs

- Behavior is less localized
- Debugging requires good tracing
- System ordering must be carefully designed

---

## 10. When ECS Becomes Worth It

ECS-style UI becomes most valuable when adding:

- animations
- theming
- accessibility
- complex interactions
- state synchronization

Before that, behavior composition is often sufficient.

---

## 11. Key Takeaway

ECS for UI means:

> Views are data.
> Behavior lives in systems.
> Widgets are just common component combinations.

This model aligns well with C and with reducer-style, state-driven UI design.

