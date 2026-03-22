#include "Matrix.h"

Vector3f cross(Vector3f &v1, Vector3f &v2)
{
    return Vector3f(v1.y*v2.z - v1.z*v2.y,
                    v1.z*v2.x - v1.x*v2.z,
                    v1.x*v2.y - v1.y*v2.x);
}