# Overview

As explained in the [Introduction](../introduction) chapter, Pyrite64 already provides an engine. \
An kind of general gameplay loop and handling of events is done purely by it. \
The way custom logic is implemented is by registering scripts in the editor. \
Whenever it is necessary, the engine will then call into user provided code.

There two main types of scripts:

## Object Scripts

If you haven't already, please read the [Game Scenes](../intro/scene) docs first.
 
Object Scripts are attached to an object in a scene. \
The main purpose is to implement individual behaviors of "things" in your scene. \
Examples can be: the player, enemies, interactable set-pieces, etc.

A script itself is "neutral", as in it can be used on different objects,\
and doesn't extend any specific class.

Instead, it will receive the attached object and data as arguments when called. \
So while written in C++, it is more similar to a C-style functional approach.

Scripts can also define a block of memory with variables to be used. \
Those are then instantiated for each object, and can also be pre-set in the editor per object.

At runtime, the script will then be called at various points (e.g. update and draw), \
and may receive events (e.g. collision) as well.

Since it is bound to an object, the lifetime of the script is bound to it too. \
Meaning when the object spawns, so will the script instance, \
and when it is destroyed the script instance will be as well.

## Global Scripts

A Global Script is not bound to any object or scene, \
but as the name suggests is global to the game.\
Pyrite64 will hook those up automatically if they exist.

The main purpose is to implement systems not easily doable via objects,\
for example general UI, or a drop-shadow helper.

They can also be used to set up global memory, \
which can for example hold a game-state like the current score or lives. \
Since they don't get destroyed during scene changes, they never lose their data.

To make everything usable, the engine calls into its callbacks at various points.\
Including during starting the game, loading a scene, before and after updating, etc.