#pragma once
#include "defs.h"

struct Vector2f
{
    float x, y;

    Vector2f(): x(0.f), y(0.f) {}
    Vector2f(float f): x(f), y(f) {}
	Vector2f(float *f): x(f[0]), y(f[1]) {}
    Vector2f(float _x, float _y): x(_x), y(_y) {}

    Vector2f operator + (Vector2f v)
    {
        return Vector2f(x+v.x, y+v.y);
    }

    Vector2f operator - ()
    {
        return Vector2f(-x, -y);
    }

    Vector2f operator - (Vector2f v)
    {
        return Vector2f(x-v.x, y-v.y);
    }

    Vector2f operator * (Vector2f v)
    {
        return Vector2f(x*v.x, y*v.y);
    }

    Vector2f operator / (Vector2f v)
    {
        return Vector2f(x/v.x, y/v.y);
    }

    Vector2f operator * (float f)
    {
        return Vector2f(x*f, y*f);
    }

    Vector2f operator / (float f)
    {
        return Vector2f(x/f, y/f);
    }
};

struct Vector3f
{
    float x, y, z;

    Vector3f(): x(0.f), y(0.f), z(0.f) {}
    Vector3f(float f): x(f), y(f), z(f) {}
	Vector3f(float *f): x(f[0]), y(f[1]), z(f[2]) {}
    Vector3f(float _x, float _y, float _z): x(_x), y(_y), z(_z) {}

    Vector3f operator + (Vector3f v)
    {
        return Vector3f(x+v.x, y+v.y, z+v.z);
    }

    Vector3f& operator += (Vector3f v)
    {
        x+=v.x;
        y+=v.y;
        z+=v.z;
        return *this;
    }

    Vector3f operator - ()
    {
        return Vector3f(-x, -y, -z);
    }

    Vector3f operator - (Vector3f v)
    {
        return Vector3f(x-v.x, y-v.y, z-v.z);
    }

    Vector3f& operator -= (Vector3f v)
    {
        x-=v.x;
        y-=v.y;
        z-=v.z;
        return *this;
    }

    Vector3f operator * (Vector3f v)
    {
        return Vector3f(x*v.x, y*v.y, z*v.z);
    }

    Vector3f operator / (Vector3f v)
    {
        return Vector3f(x/v.x, y/v.y, z/v.z);
    }

    Vector3f operator * (float f)
    {
        return Vector3f(x*f, y*f, z*f);
    }

    Vector3f operator / (float f)
    {
        return Vector3f(x/f, y/f, z/f);
    }

    void normalize()
    {
        float n = length();
        x/=n;
        y/=n;
        z/=n;
    }

	float length()
	{
		return sqrt(x*x + y*y + z*z);;
	}
    void print()
    {
        printf("[ %f\t%f\t%f]\n", x, y, z);
    }
};


Vector3f cross(Vector3f &v1, Vector3f &v2);

struct Vector4f
{
    float x, y, z, w;

    Vector4f(): x(0.f), y(0.f), z(0.f), w(0.f) {}
    Vector4f(float f): x(f), y(f), z(f), w(f) {}
	Vector4f(float *f): x(f[0]), y(f[1]), z(f[2]), w(f[3]) {}
    Vector4f(float _x, float _y, float _z): x(_x), y(_y), z(_z), w(1.f) {}
    Vector4f(float _x, float _y, float _z, float _w): x(_x), y(_y), z(_z), w(_w) {}

    Vector4f operator + (Vector4f v)
    {
        return Vector4f(x+v.x, y+v.y, z+v.z, w+v.w);
    }

    Vector4f operator - ()
    {
        return Vector4f(-x, -y, -z, -w);
    }

    Vector4f operator - (Vector4f v)
    {
        return Vector4f(x-v.x, y-v.y, z-v.z, w-v.w);
    }

    Vector4f operator * (Vector4f v)
    {
        return Vector4f(x*v.x, y*v.y, z*v.z, w*v.w);
    }

    Vector4f operator / (Vector4f v)
    {
        return Vector4f(x/v.x, y/v.y, z/v.z, w/v.w);
    }

    Vector4f operator * (float f)
    {
        return Vector4f(x*f, y*f, z*f, w*f);
    }

    Vector4f operator / (float f)
    {
        return Vector4f(x/f, y/f, z/f, w/f);
    }

    void print()
    {
        printf("[%f, %f, %f, %f]\n", x, y ,z ,w);
    }
};


struct Matrix2f
{
    float m[4];
    void identity()
    {
        m[0] = 1; m[1] = 0;
        m[2] = 0; m[3] = 1;
    }
};

struct Matrix3f
{
    float m[9];
    void identity()
    {
        m[0] = 1; m[1] = 0; m[2] = 0;
        m[3] = 0; m[4] = 1; m[5] = 0;
        m[6] = 0; m[7] = 0; m[8] = 1;
    }
};

struct Matrix4f
{
    float m[16];

    Matrix4f()
    {
        identity();
    }

    Matrix4f& identity()
    {
        m[0] = 1;  m[1] = 0;  m[2] = 0;  m[3] = 0;
        m[4] = 0;  m[5] = 1;  m[6] = 0;  m[7] = 0;
        m[8] = 0;  m[9] = 0;  m[10] = 1; m[11] = 0;
        m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;

		return *this;
    }

    Matrix4f& perspective(float _fov, float aspect, float _near, float _far)
    {
        float f = 1 / tan(_fov);

        m[0] = f/aspect;  m[1] = 0;  m[2] = 0;  m[3] = 0;
        m[4] = 0;         m[5] = f;  m[6] = 0;  m[7] = 0;
        m[8] = 0;         m[9] = 0;  m[10] = (_far+_near)/(_near-_far); m[11] = 2*_far*_near/(_near-_far);
        m[12] = 0;        m[13] = 0; m[14] = -1; m[15] = 0;
		return *this;
    }

    Matrix4f& ortho(float left, float right, float top, float bottom, float _near, float _far)
    {
        m[0] = 2/(right-left);  m[1] = 0;  m[2] = 0;  m[3] = (left+right)/(right-left);
        m[4] = 0;  m[5] = 2/(top-bottom);  m[6] = 0;  m[7] = (top+bottom)/(top-bottom);
        m[8] = 0;  m[9] = 0;  m[10] = -2/(_far - _near); m[11] = (_far+_near)/(_far-_near);
        m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;
		return *this;
    }


    Matrix4f& translation(Vector3f t)
    {
        m[0] = 1;  m[1] = 0;  m[2] = 0;  m[3] = t.x;
        m[4] = 0;  m[5] = 1;  m[6] = 0;  m[7] = t.y;
        m[8] = 0;  m[9] = 0;  m[10] = 1; m[11] = t.z;
        m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;
		return *this;
    }

    Matrix4f& rotationX(float theta)
    {
        float c = cos(theta);
        float s = sin(theta);

        m[0] = 1;  m[1] = 0;  m[2] = 0;  m[3] = 0;
        m[4] = 0;  m[5] = c;  m[6] = -s;  m[7] = 0;
        m[8] = 0;  m[9] = s;  m[10] = c; m[11] = 0;
        m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;
		return *this;
    }

    Matrix4f& rotationY(float theta)
    {
        float c = cos(theta);
        float s = sin(theta);

        m[0] = c;  m[1] = 0;  m[2] = s;  m[3] = 0;
        m[4] = 0;  m[5] = 1;  m[6] = 0;  m[7] = 0;
        m[8] = -s;  m[9] = 0;  m[10] = c; m[11] = 0;
        m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;
		return *this;
    }

    Matrix4f& rotationZ(float theta)
    {
        float c = cos(theta);
        float s = sin(theta);

        m[0] = c;  m[1] = -s;  m[2] = 0;  m[3] = 0;
        m[4] = s;  m[5] = c;  m[6] = 0;  m[7] = 0;
        m[8] = 0;  m[9] = 0;  m[10] = 1; m[11] = 0;
        m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;
		return *this;
    }


    Matrix4f operator * (Matrix4f other)
    {
        Matrix4f ret;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                ret.m[i*4 + j] = 0;
                for (int k = 0; k < 4; ++k)
                {
                    ret.m[i*4 + j] += m[i*4 + k] * other.m[k*4 + j];
                }
            }
        }
        return ret;
    }

	Vector4f operator * (Vector4f v)
	{
		Vector4f ret( m[0]*v.x +  m[4]*v.y +  m[8]*v.z +  m[12]*v.w,
					  m[1]*v.x +  m[5]*v.y +  m[9]*v.z +  m[13]*v.w,
					  m[2]*v.x +  m[6]*v.y +  m[10]*v.z +  m[14]*v.w,
					  m[3]*v.x +  m[7]*v.y +  m[11]*v.z +  m[15]*v.w);
		return ret;
	}

    Vector3f operator * (Vector3f v)
	{
		Vector3f ret( m[0]*v.x +  m[4]*v.y +  m[8]*v.z +  m[12],
					  m[1]*v.x +  m[5]*v.y +  m[9]*v.z +  m[13],
					  m[2]*v.x +  m[6]*v.y +  m[10]*v.z +  m[14]);
		return ret;
	}


    void print()
    {
        printf("[ %f\t%f\t%f\t%f\n", m[0], m[1], m[2], m[3]);
        printf("  %f\t%f\t%f\t%f\n", m[4], m[5], m[6], m[7]);
        printf("  %f\t%f\t%f\t%f\n", m[8], m[9], m[10], m[11]);
        printf("  %f\t%f\t%f\t%f]\n", m[12], m[13], m[14], m[15]);
    }

	Matrix4f& transpose()
	{
		Matrix4f tmp;

		tmp.m[0] = m[0]; tmp.m[1] = m[4]; tmp.m[2] = m[8]; tmp.m[3] = m[12];
		tmp.m[4] = m[1]; tmp.m[5] = m[5]; tmp.m[6] = m[9]; tmp.m[7] = m[13];
		tmp.m[8] = m[2]; tmp.m[9] = m[6]; tmp.m[10] = m[10]; tmp.m[11] = m[14];
		tmp.m[12] = m[3]; tmp.m[13] = m[7]; tmp.m[14] = m[11]; tmp.m[15] = m[15];

		memcpy(m, tmp.m, 16 * sizeof(float));
		return *this;
	}

	Matrix4f& fromDirection(Vector3f dir, Vector3f up)
	{
		Vector3f x, y(dir), z(up);

		x = cross(y, z);
		x.normalize();
		y.normalize();
		z.normalize();


		m[0] = x.x;  m[1] = y.x;  m[2] = z.x;   m[3] = 0;
		m[4] = x.y;  m[5] = y.y;  m[6] = z.y;   m[7] = 0;
		m[8] = x.z;  m[9] = y.z;  m[10] = z.z;  m[11] = 0;
        m[12] = 0;   m[13] = 0;   m[14] = 0;    m[15] = 1;

		return *this;
	}

    float operator () (int i, int j)
    {
        return m[(i-1)*4 + (j-1)];
    }
};
