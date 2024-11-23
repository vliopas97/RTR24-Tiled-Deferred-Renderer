# RTR 2024 Project - Deferred Renderer

This is the first submission of the project for Real-Time Rendering. So far, basic functionality has been implemented. Namely:

* Loading a scene ([Sponza Atrium](https://www.cryengine.com/asset-db/product/crytek/sponza-sample-scene))
* Basic lighting (Phong) with diffuse, normal and specular textures being loaded.\
 TBN transformations are being used for normals and alpha testing is performed to draw sprites correctly.
* For the first submission I only use one, directional light representing the sun to illuminate the whole scene, I will add more light sources locally, represented by sphere primitives in a later commit.
* Debug camera (manual-only movement support for now)
* GUI layer

## Installation

All necessary libraries are located in the *ThirdParty* folder.\
To build dependencies and project files, run the *GenerateProjects.bat* file, in the root folder.\
 The project uses DirectX12 so it only runs on Windows environments.

## Key Bindings

* <kbd>W</kbd> <kbd>A</kbd> <kbd>S</kbd> <kbd>D</kbd> and mouse to move the camera around.
* <kbd>Esc</kbd> to close the app
* <kbd>F1</kbd> mouse focus on window toggle on/off

## Third Party Libraries

* Direct X 12 as the graphics API
* [DirectXTex](https://github.com/microsoft/DirectXTex) for loading images from disk
* [ImGui](https://github.com/ocornut/imgui) for the graphical user interface layer
* [premake](https://github.com/premake/premake-core) for building project files
* [glm](https://github.com/g-truc/glm) was the math library of choice
* [Assimp](https://github.com/assimp/assimp) for importing assets
