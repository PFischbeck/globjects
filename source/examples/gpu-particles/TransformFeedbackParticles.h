#pragma once

#include <memory>

#include "AbstractParticleTechnique.h"

namespace globjects
{
class TransformFeedback;
class VertexArray;
class Program;
class Buffer;
}

class TransformFeedbackParticles : public AbstractParticleTechnique
{
public:
    TransformFeedbackParticles(
        const std::vector<glm::vec4> & positions
    ,   const std::vector<glm::vec4> & velocities
    ,   const globjects::Texture & forces
    ,   const Camera & camera);
    virtual ~TransformFeedbackParticles();

    virtual void initialize() override;
    virtual void reset() override;

    virtual void step(float elapsed) override;

protected:
    virtual void draw_impl() override;

protected:
    std::unique_ptr<globjects::TransformFeedback> m_transformFeedback;
    std::unique_ptr<globjects::Program> m_transformFeedbackProgram;

    std::unique_ptr<globjects::Buffer> m_sourcePositions;
    std::unique_ptr<globjects::Buffer> m_sourceVelocities;
    std::unique_ptr<globjects::Buffer> m_targetPositions;
    std::unique_ptr<globjects::Buffer> m_targetVelocities;

    std::unique_ptr<globjects::VertexArray> m_vao;
};
