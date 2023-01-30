#include <math.h>


struct vec3
{
	union
	{
		float data[3];
		struct
		{
			float x;
			float y;
			float z;
		};
	};
    
	vec3 operator+(vec3 rhs);
	vec3 operator+=(vec3 rhs);
	vec3 operator-(vec3 rhs);
	vec3 operator-();
	vec3 operator-=(vec3 rhs);
	vec3 operator*(float rhs);
	vec3 operator*(vec3 rhs);
	vec3 operator/(float rhs);
	vec3 vec3::operator*=(f32 rhs);
	vec3 vec3::operator/=(f32 rhs);
	bool vec3::operator==(vec3 rhs);
	bool vec3::operator!=(vec3 rhs);
};


inline vec3 Vec3(f32 f)
{
	return {f, f, f};
}

inline vec3 Vec3(f32 x, f32 y, f32 z)
{
	return {x, y, z};
}

vec3 vec3::operator+(vec3 rhs)
{
	return { this->x + rhs.x, this->y + rhs.y , this->z + rhs.z};
}

vec3 vec3::operator+=(vec3 rhs)
{
	*this = (*this + rhs);
	return *this;
}

vec3 vec3::operator-(vec3 rhs)
{
	return { this->x - rhs.x, this->y - rhs.y, this->z - rhs.z };
}

vec3 vec3::operator-()
{
	vec3 result;
	result.x = -this->x;
	result.y = -this->y;
	result.z = -this->z;
	return result;
}

vec3 vec3::operator-=(vec3 rhs)
{
	*this = (*this - rhs);
	return *this;
}

vec3 vec3::operator*(float rhs)
{
	return { this->x * rhs, this->y * rhs, this->z * rhs};
}

vec3 vec3::operator*(vec3 rhs)
{
	return {this->x * rhs.x, this->y * rhs.y, this->z * rhs.z};
};


vec3 operator*(float lhs, vec3 rhs)
{
	return { lhs * rhs.x, lhs * rhs.y,  lhs * rhs.z};
}

vec3 vec3::operator*=(f32 rhs)
{
	*this = (*this * rhs);
	return *this;
}

vec3 vec3::operator/(float rhs)
{
	return { this->x / rhs, this->y / rhs, this->z / rhs};
}

vec3 vec3::operator/=(f32 rhs)
{
	*this = (*this / rhs);
	return *this;
}

bool vec3::operator==(vec3 rhs)
{
	return (this->x == rhs.x && this->y == rhs.y && this->z == rhs.z);
}

bool vec3::operator!=(vec3 rhs)
{
	return !(*this == rhs);
}

f32 length(vec3 v) {
	return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
};

vec3 normalize(vec3 v) {
	vec3 result = v;
	f32 len = length(v);
	result.x /= len;
	result.y /= len;
	result.z /= len;
	return result;
};

inline f32 dot(vec3 v1, vec3 v2)
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}
constexpr inline vec3 cross(vec3 v1, vec3 v2)
{
	return {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x};
}

struct vec4 {
	union {
		struct {
			f32 x;
			f32 y;
			f32 z;
			f32 w;
		};
		struct {
			f32 data[4];
		};
	};
};

f32 length(vec4 v) {
	return sqrt(v.x*v.x + v.y*v.y + v.z*v.z + v.w*v.w);
};

vec4 normalize(vec4 v) {
	vec4 result = v;
	f32 len = length(v);
	result.x /= len;
	result.y /= len;
	result.z /= len;
	result.w /= len;
	return result;
};

struct Mat4x4
{
	union
	{
		f32 data[16];
		struct
		{
			f32 d[4][4];
		};
		struct
		{
			vec4 row_vecs[4];
		};
	};
};

Mat4x4 mult(Mat4x4 m1, Mat4x4 m2)
{
	Mat4x4 result;
    
	result = {};
    
	for(u32 i = 0; i < 4; ++i)
	{
		for(u32 j = 0; j < 4; ++j)
		{
			for(u32 k = 0; k < 4; ++k)
			{
				result.d[i][j] += m1.d[i][k] * m2.d[k][j];
			}
		}
	}
    
	return result;
}

vec4 mult(Mat4x4 m, vec4 v)
{
	vec4 result;
    
	result = {};
    
	for(u32 i = 0; i < 4; ++i)
	{
		u32 mIndex = i * 4;
		result.data[i] = v.x * m.data[mIndex + 0] + v.y * m.data[mIndex + 1] + v.z * m.data[mIndex + 2] + v.w * m.data[mIndex + 3];
	}
    
	return result;
}

Mat4x4 transpose(Mat4x4 m)
{
	Mat4x4 result = {};
	for(s8 i = 0; i < 4; ++i)
	{
		for(s8 j = 0; j < 4; ++j)
		{
			result.d[i][j] = m.d[j][i];
		}
	}
	return result;
}


Mat4x4 look_at(vec3 eye, vec3 target, vec3 up)
{
	vec3 f = normalize(target - eye);
	vec3 s = normalize(cross(f, up));
	vec3 u = cross(s, f);
    
	Mat4x4 result = {};
    
	result.d[0][0] = s.x;
	result.d[0][1] = s.y;
	result.d[0][2] = s.z;
	result.d[1][0] = u.x;
	result.d[1][1] = u.y;
	result.d[1][2] = u.z;
	result.d[2][0] =-f.x;
	result.d[2][1] =-f.y;
	result.d[2][2] =-f.z;
	result.d[0][3] =-dot(s, eye);
	result.d[1][3] =-dot(u, eye);
	result.d[2][3] = dot(f, eye);
	result.d[3][3] = 1.0f;
    
	return result;
}


Mat4x4 perspective_infinite_reversed_z(f32 fov_in_degrees, f32 near_plane_distance, f32 width, f32 height)
{
	Mat4x4 result = {};

	f32 fov = fov_in_degrees* 3.14159265f / 180.0f;
	f32 e = 1.0f / tanf(fov * 0.5f);
	f32 aspect = width / height;
	f32 n = near_plane_distance;

	f32 epsilon = 1e-9f;

	result.d[0][0] = e;
	result.d[1][1] = e*aspect;
	result.d[2][2] = 0.0f;
	result.d[2][3] = n;
	result.d[3][2] = epsilon-1.0f;

	return result;
}




u32 rng = 1337;
void advance_rng(u32 *rng) {
	*rng *= 1664525;
	*rng += 1013904223;
}
f32 rand_f32_normal(u32 *rng) {
	f32 result = (*rng >> 8) / 16777216.0f;   
	advance_rng(rng);    
	return result;
}
f32 rand_f32_in_range(f32 bottom_inclusive, f32 top_inclusive, u32 *rng)
{
	if (bottom_inclusive > top_inclusive)
	{
		f32 temp = top_inclusive;
		top_inclusive = bottom_inclusive;
		bottom_inclusive = temp;
	}    
	f32 range = top_inclusive - bottom_inclusive;    
	f32 normal = rand_f32_normal(rng);
	advance_rng(rng);
	return normal * range + bottom_inclusive;
}
