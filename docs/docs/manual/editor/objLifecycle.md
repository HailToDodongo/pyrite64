# Object Lifecycle & Events

This page describes what stages objects in a scene go through\
as well as what events they may receive.\
If you haven't already, please check the [Intro - Scene](../intro/scene) docs first.

## Object Creation

Before being added to a scene, the object itself is first initialized.\
This internally allocates one linear block of memory for the entire object and its components.\
Followed by running the init functions on components.\
The sequence looks like this:

```{image} /_static/img/obj_life_init.png
:align: center
:width: 640px
```

Both manually spawning an object and the initial scene load behave almost the same.\
The only difference is that a manual spawn is deferred until the next frame,\
while the initial scene load, of course, happens immediately.\
Deferring is needed to avoid unwanted modifications to the scene state while it is being processed.

The component init phase is also where any of your attached C++ scripts get called.\
First with the `init` function, if it has any, and once all components are done, with an event.

Note that inside the `init` function, the only guarantee you have is that the object itself exists.\
Meaning it is in the scene graph, and you can access its direct properties (e.g.: position).\
Depending on order, other components may not be initialized yet.\
If you need to access them, you can listen to the ready event (`EVENT_TYPE_READY`) and put additional logic there.

### Multiple Objects

So far we only considered one object,\
but during the initial scene load many more are loaded at once.\
The logic is exactly the same, where each stage just goes over all objects.

```{image} /_static/img/obj_life_init_multi.png
:align: center
:width: 640px
```

Accessing other objects during init needs the same care as accessing components.\
That is besides the object itself existing (and the ID being valid), it may not be initialized yet.

The order in which objects are processed it guaranteed to be the same as the order they are listed in the scene graph.\
There is no special handling of parent-child relationships, however,\
since children always come after their parents, the parent will be called first.

## Object Runtime

Once added to the scene, the object will now get called each frame in various situations.\
Across a frame with once again 3 objects, it may look like this:

```{image} /_static/img/frame_timeline.png
:align: center 
```
Like in the earlier graphic, keep in mind each object function is actually called per component.\
So e.g.: `update` calls the update function of all components one by one.

### Logic Phase

Spawning new objects is deferred, so if at any given point in the last frame this was attempted,\
it will now be performed at the beginning of this frame.\
As mentioned before, this avoids all sorts of side effects compared to immediately spawning it.

Similarly, any events that where sent last frame are collected and processed at the beginning of this frame.

After all of that, it's time to update the collision / physics system.\
Since it is running at a fixed timestep, it can run any number of times per frame.\
So either not at all, once, or even multiple times.\
Before each step the `fixedUpdate` function is run on all objects to interact with the physics system.
When the system itself runs after that, any collision events are then also dispatched to the objects (calling `onColl`).

With physics done, the regular `update` function is called on all objects.\
This always happens exactly once per frame, also passing in the real delta-time.

The last part of the logic update is deleting any pending objects.\
Any deletion attempt is deferred to avoid side effects and performed only at this stage.

### Drawing Phase

Now the scene gets rendered.\
Since you can have multiple cameras in a scene (e.g.: split screen)\
this might be done multiple times per frame, once for each camera.\
With each of those passes, the `draw` function is called on all objects.\
You can also get the currently active camera now from the scene.

The fact `draw` happens per camera is also important for any visibility logic.\
For example, the culling component internally runs inside draw to handle this.
