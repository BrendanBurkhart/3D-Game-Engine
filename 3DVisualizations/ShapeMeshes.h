#pragma once

#include "Mesh.h"

class ShapeMeshes
{
public:
    static Mesh Cube();
    static Mesh Dodecahedron();
    static Mesh EmbeddedCube(int i, bool large);
};
