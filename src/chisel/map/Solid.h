#pragma once

#include "console/ConVar.h"
#include "chisel/Selection.h"
#include "assets/Assets.h"
#include "render/Render.h"
#include "Orientation.h"
#include "Atom.h"

#include "math/Color.h"

#include "Common.h"
#include "Displacement.h"

#include <memory>
#include <unordered_map>
#include <array>

namespace chisel
{
    // TODO: Move to some transform state?
    extern ConVar<bool>  trans_texture_lock;
    extern ConVar<bool>  trans_texture_scale_lock;
    extern ConVar<bool>  trans_texture_face_alignment;

    struct Side
    {
        Side()
        {
        }

        Side(const Plane& plane, Rc<Material> material, float scale = 0.25f)
            : plane{ plane }
            , material{ std::move(material) }
            , scale{ scale, scale }
        {
            Orientation orientation = Orientations::CalcOrientation(plane);
            if (orientation == Orientations::Invalid)
                return;

            textureAxes[1].xyz = Orientations::DownVectors[orientation];
            if (trans_texture_face_alignment)
            {
                // Calculate true U axis
                textureAxes[0].xyz = glm::normalize(
                    glm::cross(glm::vec3(textureAxes[1].xyz), plane.normal));

                // Now calculate the true V axis
                textureAxes[1].xyz = glm::normalize(
                    glm::cross(plane.normal, glm::vec3(textureAxes[0].xyz)));
            }
            else
            {
                textureAxes[0].xyz = Orientations::RightVectors[orientation];
            }
        }

        Plane plane{};

        Rc<Material> material;
        std::array<vec4, 2> textureAxes { vec4(0.0f), vec4(0.0f) };
        std::array<float, 2> scale { 1.0f, 1.0f };
        float rotate = 0;
        float lightmapScale = 16;
        uint32_t smoothing = 0;
        std::optional<DispInfo> disp;
    };

    struct Solid;
    struct BrushEntity;

    struct BrushMesh
    {
        std::vector<VertexSolid> vertices;
        std::vector<uint32_t>    indices;

        std::optional<BrushGPUAllocator::Allocation> alloc;
        Material *material = nullptr;
        Solid *brush = nullptr;
    };

    struct Face
    {
        Face(Side* side, std::vector<vec3> points)
            : side(side)
            , points(std::move(points))
        {}

        Face(Face&& other) = default;
        Face(const Face& other) = default;
        Face& operator=(const Face& other) = default;

        Side* side;
        std::vector<vec3> points;
    };

    class Solid : public Atom
    {
    public:
        Solid(BrushEntity* parent);
        Solid(BrushEntity* parent, std::vector<Side> sides, bool initMesh = true);
        Solid(Solid&& other);
        ~Solid();

        // What the fuck, why do I need this?
        bool operator == (const Solid& other) const
        {
            return this == &other;
        }

        bool HasDisplacement() const { return m_displacement; }
        std::vector<BrushMesh>& GetMeshes() { return m_meshes; }
        const std::vector<Side>& GetSides() const { return m_sides; }
        const std::vector<Face>& GetFaces() const { return m_faces; }

        void Clip(Side side); // Remember to UpdateMesh after this!

        void UpdateMesh();


    // Selectable Interface //

        std::optional<AABB> GetBounds() const final override { return m_bounds; }

        void Transform(const mat4x4& _matrix) final override;
        void AlignToGrid(vec3 gridSize) final override;

        Selectable* ResolveSelectable() final override;
        bool IsSelected() const final override;

        void Delete() final override;

    private:

        bool m_displacement = false;

        std::vector<BrushMesh> m_meshes;
        std::vector<Side> m_sides;
        std::optional<AABB> m_bounds;

        std::vector<Face> m_faces;
    };

    std::vector<Side> CreateCubeBrush(Material* material, vec3 size = vec3(64.f), const mat4x4& transform = glm::identity<mat4x4>());
}