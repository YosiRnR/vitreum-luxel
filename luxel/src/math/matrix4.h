#pragma once
#include "vectors.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace luxel::math
{
    struct Matrix4x4
    {
        // Almacenamiento continuo en memoria (Column-Major)
        // [0][1][2][3]    -> Columna 0
        // [4][5][6][7]    -> Columna 1
        // [8][9][10][11]  -> Columna 2
        // [12][13][14][15]-> Columna 3 (habitualmente la traslación)
        std::array<float, 16> m{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        Matrix4x4() = default;
        constexpr explicit Matrix4x4(const std::array<float, 16>& values) noexcept : m(values) {}

        [[nodiscard]] static constexpr Matrix4x4 Zero() noexcept {
            Matrix4x4 z;
            std::ranges::fill(z.m, 0.f);
            return z;
        }

        // Matriz identidad
        [[nodiscard]] static constexpr Matrix4x4 Identity() noexcept {
            return Matrix4x4{};
        }

        // Matriz inversa (solo para matrices de transformación 2D/3D simples)
        [[nodiscard]] Matrix4x4 Inverted() const noexcept
        {
            Matrix4x4 inv = *this;
            inv.Invert();
            return inv;
        }

        bool Invert()
        {
            // Accedemos a los 16 floats de la matriz actual
            const float* mptr = m.data();
            std::array<float, 16> inv{};

            inv[0] = mptr[5] * mptr[10] * mptr[15] -
                mptr[5] * mptr[11] * mptr[14] -
                mptr[9] * mptr[6] * mptr[15] +
                mptr[9] * mptr[7] * mptr[14] +
                mptr[13] * mptr[6] * mptr[11] -
                mptr[13] * mptr[7] * mptr[10];

            inv[4] = -mptr[4] * mptr[10] * mptr[15] +
                mptr[4] * mptr[11] * mptr[14] +
                mptr[8] * mptr[6] * mptr[15] -
                mptr[8] * mptr[7] * mptr[14] -
                mptr[12] * mptr[6] * mptr[11] +
                mptr[12] * mptr[7] * mptr[10];

            inv[8] = mptr[4] * mptr[9] * mptr[15] -
                mptr[4] * mptr[11] * mptr[13] -
                mptr[8] * mptr[5] * mptr[15] +
                mptr[8] * mptr[7] * mptr[13] +
                mptr[12] * mptr[5] * mptr[11] -
                mptr[12] * mptr[7] * mptr[9];

            inv[12] = -mptr[4] * mptr[9] * mptr[14] +
                mptr[4] * mptr[10] * mptr[13] +
                mptr[8] * mptr[5] * mptr[14] -
                mptr[8] * mptr[6] * mptr[13] -
                mptr[12] * mptr[5] * mptr[10] +
                mptr[12] * mptr[6] * mptr[9];

            inv[1] = -mptr[1] * mptr[10] * mptr[15] +
                mptr[1] * mptr[11] * mptr[14] +
                mptr[9] * mptr[2] * mptr[15] -
                mptr[9] * mptr[3] * mptr[14] -
                mptr[13] * mptr[2] * mptr[11] +
                mptr[13] * mptr[3] * mptr[10];

            inv[5] = mptr[0] * mptr[10] * mptr[15] -
                mptr[0] * mptr[11] * mptr[14] -
                mptr[8] * mptr[2] * mptr[15] +
                mptr[8] * mptr[3] * mptr[14] +
                mptr[12] * mptr[2] * mptr[11] -
                mptr[12] * mptr[3] * mptr[10];

            inv[9] = -mptr[0] * mptr[9] * mptr[15] +
                mptr[0] * mptr[11] * mptr[13] +
                mptr[8] * mptr[1] * mptr[15] -
                mptr[8] * mptr[3] * mptr[13] -
                mptr[12] * mptr[1] * mptr[11] +
                mptr[12] * mptr[3] * mptr[9];

            inv[13] = mptr[0] * mptr[9] * mptr[14] -
                mptr[0] * mptr[10] * mptr[13] -
                mptr[8] * mptr[1] * mptr[14] +
                mptr[8] * mptr[2] * mptr[13] +
                mptr[12] * mptr[1] * mptr[10] -
                mptr[12] * mptr[2] * mptr[9];

            inv[2] = mptr[1] * mptr[6] * mptr[15] -
                mptr[1] * mptr[7] * mptr[14] -
                mptr[5] * mptr[2] * mptr[15] +
                mptr[5] * mptr[3] * mptr[14] +
                mptr[13] * mptr[2] * mptr[7] -
                mptr[13] * mptr[3] * mptr[6];

            inv[6] = -mptr[0] * mptr[6] * mptr[15] +
                mptr[0] * mptr[7] * mptr[14] +
                mptr[4] * mptr[2] * mptr[15] -
                mptr[4] * mptr[3] * mptr[14] -
                mptr[12] * mptr[2] * mptr[7] +
                mptr[12] * mptr[3] * mptr[6];

            inv[10] = mptr[0] * mptr[5] * mptr[15] -
                mptr[0] * mptr[7] * mptr[13] -
                mptr[4] * mptr[1] * mptr[15] +
                mptr[4] * mptr[3] * mptr[13] +
                mptr[12] * mptr[1] * mptr[7] -
                mptr[12] * mptr[3] * mptr[5];

            inv[14] = -mptr[0] * mptr[5] * mptr[14] +
                mptr[0] * mptr[6] * mptr[13] +
                mptr[4] * mptr[1] * mptr[14] -
                mptr[4] * mptr[2] * mptr[13] -
                mptr[12] * mptr[1] * mptr[6] +
                mptr[12] * mptr[2] * mptr[5];

            inv[3] = -mptr[1] * mptr[6] * mptr[11] +
                mptr[1] * mptr[7] * mptr[10] +
                mptr[5] * mptr[2] * mptr[11] -
                mptr[5] * mptr[3] * mptr[10] -
                mptr[9] * mptr[2] * mptr[7] +
                mptr[9] * mptr[3] * mptr[6];

            inv[7] = mptr[0] * mptr[6] * mptr[11] -
                mptr[0] * mptr[7] * mptr[10] -
                mptr[4] * mptr[2] * mptr[11] +
                mptr[4] * mptr[3] * mptr[10] +
                mptr[8] * mptr[2] * mptr[7] -
                mptr[8] * mptr[3] * mptr[6];

            inv[11] = -mptr[0] * mptr[5] * mptr[11] +
                mptr[0] * mptr[7] * mptr[9] +
                mptr[4] * mptr[1] * mptr[11] -
                mptr[4] * mptr[3] * mptr[9] -
                mptr[8] * mptr[1] * mptr[7] +
                mptr[8] * mptr[3] * mptr[5];

            inv[15] = mptr[0] * mptr[5] * mptr[10] -
                mptr[0] * mptr[6] * mptr[9] -
                mptr[4] * mptr[1] * mptr[10] +
                mptr[4] * mptr[2] * mptr[9] +
                mptr[8] * mptr[1] * mptr[6] -
                mptr[8] * mptr[2] * mptr[5];

            // Calculamos el Determinante
            float det = mptr[0] * inv[0] + mptr[1] * inv[4] + mptr[2] * inv[8] + mptr[3] * inv[12];

            // Si el determinante es 0, la matriz no tiene inversa (ej. escala 0)
            if (det == 0.0f) {
                return false;
            }

            float invDet = 1.0f / det;

            for (int i = 0; i < 16; i++) {
                m[i] = inv[i] * invDet;
            }

            return true;
        }

        // Puntero directo a los datos
        [[nodiscard]] constexpr const float* data() const noexcept {
            return m.data();
        }

        // Multiplicación de matrices
        [[nodiscard]] friend constexpr Matrix4x4 operator*(const Matrix4x4& lhs, const Matrix4x4& rhs) noexcept
        {
            Matrix4x4 result{};
            for (size_t row = 0; row < 4; ++row)
            {
                for (size_t col = 0; col < 4; ++col)
                {
                    result.m[col * 4 + row] =
                        lhs.m[0 * 4 + row] * rhs.m[col * 4 + 0] +
                        lhs.m[1 * 4 + row] * rhs.m[col * 4 + 1] +
                        lhs.m[2 * 4 + row] * rhs.m[col * 4 + 2] +
                        lhs.m[3 * 4 + row] * rhs.m[col * 4 + 3];
                }
            }
            return result;
        }

        // Traslación
        [[nodiscard]] static Matrix4x4 translate(const Vector3f& v) noexcept
        {
            Matrix4x4 result = Identity();
            // En Column-Major, la última columna (índices 12, 13, 14) guarda la posición XYZ
            result.m[12] = v.x;
            result.m[13] = v.y;
            result.m[14] = v.z;
            return result;
        }

        [[nodiscard]] static Matrix4x4 rotateX(float angleRadians) noexcept
        {
            Matrix4x4 result = Identity();
            const float c = std::cos(angleRadians);
            const float s = std::sin(angleRadians);
            result.m[5] = c; result.m[9] = -s;
            result.m[6] = s; result.m[10] = c;

            return result;
        }

        // Rotación alrededor del eje Y
        [[nodiscard]] static Matrix4x4 rotateY(float angleRadians) noexcept
        {
            Matrix4x4 result = Identity();
            const float c = std::cos(angleRadians);
            const float s = std::sin(angleRadians);
            result.m[0] = c;  result.m[8] = s;
            result.m[2] = -s; result.m[10] = c;
            return result;
        }

        // Rotación alrededor del eje Z (2D)
        [[nodiscard]] static Matrix4x4 rotateZ(float angleRadians) noexcept
        {
            Matrix4x4 result = Identity();
            const float c = std::cos(angleRadians);
            const float s = std::sin(angleRadians);
            result.m[0] = c; result.m[4] = -s;
            result.m[1] = s; result.m[5] = c;
            return result;
        }

        // Escalado
        [[nodiscard]] static Matrix4x4 scale(const Vector3f& v) noexcept
        {
            Matrix4x4 result = Identity();
            result.m[0] = v.x; // Escalado en X
            result.m[5] = v.y; // Escalado en Y
            result.m[10] = v.z; // Escalado en Z
            return result;
        }

        // Proyección ortográfica
        [[nodiscard]] static Matrix4x4 orthographic(float left, float right, float bottom, float top, float zNear = -1.0f, float zFar = 1.0f) noexcept
        {
            Matrix4x4 result{};

            result.m[0] = 2.0f / (right - left);
            result.m[5] = 2.0f / (top - bottom);
            result.m[10] = -2.0f / (zFar - zNear);

            result.m[12] = -(right + left) / (right - left);
            result.m[13] = -(top + bottom) / (top - bottom);
            result.m[14] = -(zFar + zNear) / (zFar - zNear);
            result.m[15] = 1.0f;

            return result;
        }

        // Proyección 3D
        [[nodiscard]] static Matrix4x4 perspective(float fovRadians, float aspect, float nearClip, float farClip) noexcept
        {
            Matrix4x4 result = Zero(); // Inicializa a 0.f

            const float halfTanFov = std::tan(fovRadians * 0.5f);

            result.m[0] = 1.0f / (aspect * halfTanFov);
            result.m[5] = 1.0f / halfTanFov; // Nota: En Vulkan, si quieres invertir el eje Y directamente en la proyección, puedes usar -1.0f / halfTanFov
            result.m[10] = farClip / (nearClip - farClip);
            result.m[11] = -1.0f;
            result.m[14] = (nearClip * farClip) / (nearClip - farClip);

            return result;
        }

        // =====================================================================
        // Multiplicación de Matriz 4x4 por Vector4f
        // =====================================================================
        [[nodiscard]] friend constexpr Vector4f operator*(const Matrix4x4& mat, const Vector4f& v) noexcept
        {
            return Vector4f{
                mat.m[0] * v.x + mat.m[4] * v.y + mat.m[8] * v.z + mat.m[12] * v.w,
                mat.m[1] * v.x + mat.m[5] * v.y + mat.m[9] * v.z + mat.m[13] * v.w,
                mat.m[2] * v.x + mat.m[6] * v.y + mat.m[10] * v.z + mat.m[14] * v.w,
                mat.m[3] * v.x + mat.m[7] * v.y + mat.m[11] * v.z + mat.m[15] * v.w
            };
        }

        // =====================================================================
        // Multiplicación de Matriz 4x4 por Vector3f (Punto 3D/2D, asume w = 1.0f)
        // =====================================================================
        [[nodiscard]] friend constexpr Vector3f operator*(const Matrix4x4& mat, const Vector3f& v) noexcept
        {
            return Vector3f{
                mat.m[0] * v.x + mat.m[4] * v.y + mat.m[8] * v.z + mat.m[12],
                mat.m[1] * v.x + mat.m[5] * v.y + mat.m[9] * v.z + mat.m[13],
                mat.m[2] * v.x + mat.m[6] * v.y + mat.m[10] * v.z + mat.m[14]
            };
        }

        // =====================================================================
        // Transformar un punto 2D (Vector2f, asume z = 0.0f, w = 1.0f)
        // =====================================================================
        [[nodiscard]] constexpr Vector2f TransformPoint(const Vector2f& p) const noexcept
        {
            return Vector2f{
                m[0] * p.x + m[4] * p.y + m[12],
                m[1] * p.x + m[5] * p.y + m[13]
            };
        }

        // =====================================================================
        // Transformar una dirección/vector 2D (Vector2f, asume z = 0.0f, w = 0.0f, ignora traslación)
        // =====================================================================
        [[nodiscard]] constexpr Vector2f TransformDirection(const Vector2f& d) const noexcept
        {
            return Vector2f{
                m[0] * d.x + m[4] * d.y,
                m[1] * d.x + m[5] * d.y
            };
        }
    };

} // namespace luxel::math
