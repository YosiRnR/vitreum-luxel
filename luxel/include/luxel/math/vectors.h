#pragma once
#include <cassert>
#include <cmath>
#include <ostream>
#include <algorithm> // Para std::clamp

namespace luxel::math
{
    struct Vector2f
    {
        float x{ 0.0f };
        float y{ 0.0f };

        Vector2f() = default;
        constexpr Vector2f(float x, float y) noexcept : x(x), y(y) {}

        // Operaciones aritméticas básicas (In-place)
        constexpr Vector2f& operator+=(const Vector2f& other) noexcept {
            x += other.x; y += other.y; return *this;
        }
        constexpr Vector2f& operator-=(const Vector2f& other) noexcept {
            x -= other.x; y -= other.y; return *this;
        }
        constexpr Vector2f& operator*=(float scalar) noexcept {
            x *= scalar; y *= scalar; return *this;
        }
        constexpr Vector2f& operator/=(float scalar) noexcept {
            assert(scalar != 0.0f && "Vector2f::operator/= division by zero");
            x /= scalar; y /= scalar; return *this;
        }
        constexpr Vector2f operator-() const noexcept {
            return Vector2f(-x, -y);
        }
        constexpr bool operator==(const Vector2f&) const noexcept = default;
        [[nodiscard]] bool nearlyEquals(const Vector2f& other, float epsilon = 1e-6f) const noexcept
        {
            return
                std::fabs(x - other.x) <= epsilon &&
                std::fabs(y - other.y) <= epsilon;
        }

        // Operaciones comunes
        [[nodiscard]] constexpr float lengthSquared() const noexcept {
            return x * x + y * y;
        }
        [[nodiscard]] float length() const noexcept {
            return std::sqrt(lengthSquared());
        }

        [[nodiscard]] Vector2f normalized() const noexcept
        {
            constexpr float epsilon = 1e-6f; // Umbral para evitar división por cero
            const float len = length();

            return len > epsilon ? Vector2f(x / len, y / len) : Vector2f{};
        }
        Vector2f& normalize() noexcept
        {
            *this = normalized();
            return *this;
        }

        // --- PATRÓN HIDDEN FRIENDS ---
        // Operadores friend: Producto punto (dot product)
        friend constexpr float dot(const Vector2f& lhs, const Vector2f& rhs) noexcept {
            return lhs.x * rhs.x + lhs.y * rhs.y;
        }
        // Producto vectorial (cross product) para 2D, devuelve un escalar que representa el área del paralelogramo formado por los vectores
        // Coincide con el cálculo del determinante de la matriz formada por los vectores y es útil para determinar la orientación relativa de los vectores
        // y detectar si un punto está dentro de un triángulo por ejemplo
        friend constexpr float cross(const Vector2f& lhs, const Vector2f& rhs) noexcept {
            return lhs.x * rhs.y - lhs.y * rhs.x;
        }

        // Operadores binarios para Vector2f
        [[nodiscard]] friend constexpr Vector2f operator+(Vector2f lhs, const Vector2f& rhs) noexcept { return lhs += rhs; }
        [[nodiscard]] friend constexpr Vector2f operator-(Vector2f lhs, const Vector2f& rhs) noexcept { return lhs -= rhs; }
        [[nodiscard]] friend constexpr Vector2f operator*(Vector2f lhs, float scalar) noexcept { return lhs *= scalar; }
        [[nodiscard]] friend constexpr Vector2f operator*(float scalar, Vector2f rhs) noexcept { return rhs *= scalar; }
        [[nodiscard]] friend constexpr Vector2f operator/(Vector2f lhs, float scalar) noexcept { return lhs /= scalar; }

        [[nodiscard]] friend constexpr Vector2f lerp(const Vector2f& a, const Vector2f& b, float t) noexcept {
            return a + (b - a) * t;
        }
        [[nodiscard]] friend constexpr Vector2f reflect(const Vector2f& incident, const Vector2f& normal) noexcept {
            return incident - 2.0f * dot(incident, normal) * normal;
        }
        [[nodiscard]] friend constexpr float distanceSquared(const Vector2f& a, const Vector2f& b) noexcept {
            return (b - a).lengthSquared();
        }
        [[nodiscard]] friend float distance(const Vector2f& a, const Vector2f& b) noexcept {
            return (b - a).length();
        }
        [[nodiscard]] friend constexpr Vector2f clamp(const Vector2f& v, const Vector2f& lo, const Vector2f& hi) noexcept {
            return {
                std::clamp(v.x, lo.x, hi.x),
                std::clamp(v.y, lo.y, hi.y)
            };
        }
        friend std::ostream& operator<<(std::ostream& os, const Vector2f& v) {
            return os << "(" << v.x << ", " << v.y << ")";
        }
    };

    // =========================================================================

    struct Vector3f
    {
        float x{ 0.0f };
        float y{ 0.0f };
        float z{ 0.0f };

        Vector3f() = default;
        constexpr Vector3f(float x, float y, float z) noexcept : x(x), y(y), z(z) {}

        constexpr Vector3f& operator+=(const Vector3f& other) noexcept {
            x += other.x; y += other.y; z += other.z; return *this;
        }
        constexpr Vector3f& operator-=(const Vector3f& other) noexcept {
            x -= other.x; y -= other.y; z -= other.z; return *this;
        }
        constexpr Vector3f& operator*=(float scalar) noexcept {
            x *= scalar; y *= scalar; z *= scalar; return *this;
        }
        constexpr Vector3f& operator/=(float scalar) noexcept {
            assert(scalar != 0.0f && "Vector3f::operator/= division by zero");
            x /= scalar; y /= scalar; z /= scalar; return *this;
        }
        constexpr Vector3f operator-() const noexcept {
            return Vector3f(-x, -y, -z);
        }
        constexpr bool operator==(const Vector3f&) const noexcept = default;
        [[nodiscard]] bool nearlyEquals(const Vector3f& other, float epsilon = 1e-6f) const noexcept
        {
            return
                std::fabs(x - other.x) <= epsilon &&
                std::fabs(y - other.y) <= epsilon &&
                std::fabs(z - other.z) <= epsilon;
        }

        [[nodiscard]] constexpr float lengthSquared() const noexcept {
            return x * x + y * y + z * z;
        }
        [[nodiscard]] float length() const noexcept {
            return std::sqrt(lengthSquared());
        }

        [[nodiscard]] Vector3f normalized() const noexcept
        {
            constexpr float epsilon = 1e-6f; // Umbral para evitar división por cero
            const float len = length();

            return (len > epsilon) ? Vector3f(x / len, y / len, z / len) : Vector3f{};
        }
        Vector3f& normalize() noexcept
        {
            *this = normalized();
            return *this;
        }

        // --- PATRÓN HIDDEN FRIENDS ---
        friend constexpr float dot(const Vector3f& lhs, const Vector3f& rhs) noexcept {
            return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
        }

        // Operador Amigo: Producto cruz (Cross Product) - Vital para 3D y normales
        friend constexpr Vector3f cross(const Vector3f& lhs, const Vector3f& rhs) noexcept {
            return Vector3f(
                lhs.y * rhs.z - lhs.z * rhs.y,
                lhs.z * rhs.x - lhs.x * rhs.z,
                lhs.x * rhs.y - lhs.y * rhs.x
            );
        }

        // Operadores binarios para Vector3f
        [[nodiscard]] friend constexpr Vector3f operator+(Vector3f lhs, const Vector3f& rhs) noexcept { return lhs += rhs; }
        [[nodiscard]] friend constexpr Vector3f operator-(Vector3f lhs, const Vector3f& rhs) noexcept { return lhs -= rhs; }
        [[nodiscard]] friend constexpr Vector3f operator*(Vector3f lhs, float scalar) noexcept { return lhs *= scalar; }
        [[nodiscard]] friend constexpr Vector3f operator*(float scalar, Vector3f rhs) noexcept { return rhs *= scalar; }
        [[nodiscard]] friend constexpr Vector3f operator/(Vector3f lhs, float scalar) noexcept { return lhs /= scalar; }

        [[nodiscard]] friend constexpr Vector3f lerp(const Vector3f& a, const Vector3f& b, float t) noexcept {
            return a + (b - a) * t;
        }
        [[nodiscard]] friend constexpr Vector3f reflect(const Vector3f& incident, const Vector3f& normal) noexcept {
            return incident - 2.0f * dot(incident, normal) * normal;
        }
        [[nodiscard]] friend constexpr float distanceSquared(const Vector3f& a, const Vector3f& b) noexcept {
            return (b - a).lengthSquared();
        }
        [[nodiscard]] friend float distance(const Vector3f& a, const Vector3f& b) noexcept {
            return (b - a).length();
        }
        [[nodiscard]] friend constexpr Vector3f clamp(const Vector3f& v, const Vector3f& lo, const Vector3f& hi) noexcept {
            return {
                std::clamp(v.x, lo.x, hi.x),
                std::clamp(v.y, lo.y, hi.y),
                std::clamp(v.z, lo.z, hi.z)
            };
        }
        friend std::ostream& operator<<(std::ostream& os, const Vector3f& v) {
            return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
        }
    };

    // =========================================================================

    struct Vector4f
    {
        float x{ 0.0f };
        float y{ 0.0f };
        float z{ 0.0f };
        float w{ 0.0f };

        Vector4f() = default;
        constexpr Vector4f(float x, float y, float z, float w = 1.0f) noexcept
            : x(x), y(y), z(z), w(w) {
        }

        // Constructor de conversión conveniente desde Vector3f
        constexpr explicit Vector4f(const Vector3f& v3, float w = 1.0f) noexcept
            : x(v3.x), y(v3.y), z(v3.z), w(w) {
        }

        // Extrae los componentes xyz sin tocar w
        [[nodiscard]] constexpr Vector3f xyz() const noexcept {
            return { x, y, z };
        }
        // Proyecta de coordenadas homogéneas a 3D dividiendo por w
        [[nodiscard]] Vector3f toVector3() const noexcept {
            return (w != 0.0f && w != 1.0f) ? Vector3f{ x / w, y / w, z / w } : Vector3f{ x, y, z };
        }

        // Operadores in-place
        constexpr Vector4f& operator+=(const Vector4f& other) noexcept {
            x += other.x; y += other.y; z += other.z; w += other.w; return *this;
        }
        constexpr Vector4f& operator-=(const Vector4f& other) noexcept {
            x -= other.x; y -= other.y; z -= other.z; w -= other.w; return *this;
        }
        constexpr Vector4f& operator*=(float scalar) noexcept {
            x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this;
        }
        constexpr Vector4f& operator/=(float scalar) noexcept {
            assert(scalar != 0.0f && "Vector4f::operator/= division by zero");
            x /= scalar; y /= scalar; z /= scalar; w /= scalar; return *this;
        }
        constexpr Vector4f operator-() const noexcept {
            return Vector4f(-x, -y, -z, -w);
        }
        constexpr bool operator==(const Vector4f&) const noexcept = default;
        [[nodiscard]] bool nearlyEquals(const Vector4f& other, float epsilon = 1e-6f) const noexcept
        {
            return
                std::fabs(x - other.x) <= epsilon &&
                std::fabs(y - other.y) <= epsilon &&
                std::fabs(z - other.z) <= epsilon &&
                std::fabs(w - other.w) <= epsilon;
        }

        [[nodiscard]] constexpr float lengthSquared() const noexcept {
            return x * x + y * y + z * z + w * w;
        }
        [[nodiscard]] float length() const noexcept {
            return std::sqrt(lengthSquared());
        }

        [[nodiscard]] Vector4f normalized() const noexcept
        {
            constexpr float epsilon = 1e-6f; // Umbral para evitar división por cero
            const float len = length();

            return (len > epsilon) ? Vector4f(x / len, y / len, z / len, w / len) : Vector4f{};
        }

        // --- PATRÓN HIDDEN FRIENDS (Operadores Binarios) ---
        friend constexpr float dot(const Vector4f& lhs, const Vector4f& rhs) noexcept {
            return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
        }

        [[nodiscard]] friend constexpr Vector4f operator+(Vector4f lhs, const Vector4f& rhs) noexcept {
            return lhs += rhs;
        }

        [[nodiscard]] friend constexpr Vector4f operator-(Vector4f lhs, const Vector4f& rhs) noexcept {
            return lhs -= rhs;
        }

        [[nodiscard]] friend constexpr Vector4f operator*(Vector4f lhs, float scalar) noexcept {
            return lhs *= scalar;
        }

        [[nodiscard]] friend constexpr Vector4f operator*(float scalar, Vector4f rhs) noexcept {
            return rhs *= scalar;
        }

        [[nodiscard]] friend constexpr Vector4f operator/(Vector4f lhs, float scalar) noexcept {
            return lhs /= scalar;
        }

        friend std::ostream& operator<<(std::ostream& os, const Vector4f& v) {
            return os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
        }
    };

} // namespace luxel::math
