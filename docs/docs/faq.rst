FAQ
====================================
 
.. Caution:: Please read this before using this project, or asking for any support!

====================
Do I need to know programming?
====================
| Yes, C++ / C knowledge is **required**.
| Generic visual programming languages are not implemented.
| The node-graph shown in the repo / video is *only* for scripting sequences of events. 

====================
Can I make 2D games?
====================
| No.
| Any kind of 2D support is still WIP and is currently done in code.
 
====================
Can I export to PC?
====================
| No.
| The only target is N64.

====================
Can I mod existing N64 games?
====================
| No.
| This project is only for making new homebrew.

====================
Can I emulate games in Project64 / Android? 
====================
| No.
| Games made with Pyrite64 require *accurate* emulation.
| Recommended emulators are: Ares_ (v147 or newer) or gopher64_.

====================
Can I import any 3D models from blender?
====================
| Almost, materials need to be made with fast64 however.
| 
| Use Blender 4.0+ (not 5.0!), and install Fast64_.
| For correct export settings, check this note here:
| https://github.com/HailToDodongo/tiny3d?tab=readme-ov-file#gltf-model-import
| Make sure textures are .png files present in the `assets` folder of your project.
| The `tiny3d <https://github.com/HailToDodongo/tiny3d/tree/main/examples>`__ repo also contains example .blend files.

====================
Can I sell games made with it?
====================

| **I'm not a lawyer, and the following is not legal advice.**
|
| Pyrite64 itself is licensed under the `MIT License <https://github.com/HailToDodongo/pyrite64/blob/main/LICENSE>`__,
| and I put no restrictions on what you can do with games.
| The SDKs used, namely libdragon and tiny3d, have similar licenses.
| Pyrite64 does **NOT** use any proprietary SDKs.
| So in general, it should be safe to sell games made with Pyrite64 if you wish.

.. _Ares: https://ares-emu.net/
.. _gopher64: https://github.com/gopher64/gopher64
.. _Fast64: https://github.com/Fast-64/fast64

====================
Can i put games on a physical cartridge?
====================

| Yes!
| Pyrite64 produces playable ROMs that can be put on flashcards.
| The SDK used (libdragon) assumes a CIC 6102.

| Once again, Pyrite64 does **NOT** use any proprietary SDKs. 
| So the IPL3 is open source and **NOT** taken from a game.