
#include <glbinding/gl/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <globjects/Uniform.h>
#include <globjects/Program.h>
#include <globjects/Shader.h>
#include <globjects/Buffer.h>
#include <globjects/Framebuffer.h>
#include <globjects/VertexArray.h>
#include <globjects/Texture.h>

#include <common/AxisAlignedBoundingBox.h>
#include <common/Icosahedron.h>
#include <common/Camera.h>
#include <common/AbstractCoordinateProvider.h>
#include <common/WorldInHandNavigation.h>
#include <common/ScreenAlignedQuad.h>

#include <common/ContextFormat.h>
#include <common/Context.h>
#include <common/Window.h>
#include <common/WindowEventHandler.h>
#include <common/events.h>


using namespace gl;
using namespace glm;
using namespace globjects;

class EventHandler : public WindowEventHandler, AbstractCoordinateProvider
{
public:
    EventHandler()
    : m_camera(vec3(0.f, 1.f, 4.f))
    {
        m_aabb.extend(vec3(-8.f, -1.f, -8.f));
        m_aabb.extend(vec3( 8.f,  1.f,  8.f));

        m_nav.setCamera(&m_camera);
        m_nav.setCoordinateProvider(this);
        m_nav.setBoundaryHint(m_aabb);
    }

    virtual ~EventHandler()
    {
    }

    virtual void initialize(Window & window) override
    {
        WindowEventHandler::initialize(window);

        glClearColor(1.f, 1.f, 1.f, 0.f);


        m_icosahedron.reset(new Icosahedron(2));

        m_sphere.reset(new Program());

        m_sphere->attach(
            Shader::fromFile(GL_VERTEX_SHADER,   "data/gbuffers/sphere.vert"),
            Shader::fromFile(GL_FRAGMENT_SHADER, "data/gbuffers/sphere.frag"));

        m_colorTexture.reset(Texture::createDefault(GL_TEXTURE_2D));
        m_depthTexture.reset(Texture::createDefault(GL_TEXTURE_2D));
        m_normalTexture.reset(Texture::createDefault(GL_TEXTURE_2D));
        m_geometryTexture.reset(Texture::createDefault(GL_TEXTURE_2D));

        m_sphereFBO.reset(new Framebuffer);
        m_sphereFBO->attachTexture(GL_COLOR_ATTACHMENT0, m_colorTexture.get());
        m_sphereFBO->attachTexture(GL_COLOR_ATTACHMENT1, m_normalTexture.get());
        m_sphereFBO->attachTexture(GL_COLOR_ATTACHMENT2, m_geometryTexture.get());
        m_sphereFBO->attachTexture(GL_DEPTH_ATTACHMENT,  m_depthTexture.get());
        m_sphereFBO->setDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });

        m_postprocessing.reset(new ScreenAlignedQuad(
            Shader::fromFile(GL_FRAGMENT_SHADER, "data/gbuffers/postprocessing.frag")));
        m_postprocessing->program()->setUniform<GLint>("colorSource",      0);
        m_postprocessing->program()->setUniform<GLint>("normalSource",     1);
        m_postprocessing->program()->setUniform<GLint>("worldCoordSource", 2);
        m_postprocessing->program()->setUniform<GLint>("depthSource",      3);

        m_postprocessedTexture.reset(Texture::createDefault(GL_TEXTURE_2D));

        m_postprocessingFBO.reset(new Framebuffer);
        m_postprocessingFBO->attachTexture(GL_COLOR_ATTACHMENT0, m_postprocessedTexture.get());
        m_postprocessingFBO->setDrawBuffer(GL_COLOR_ATTACHMENT0);

        m_gBufferChoice.reset(new ScreenAlignedQuad(
            Shader::fromFile(GL_FRAGMENT_SHADER, "data/gbuffers/gbufferchoice.frag")));
        m_gBufferChoice->program()->setUniform<GLint>("postprocessedSource", 0);
        m_gBufferChoice->program()->setUniform<GLint>("colorSource",         1);
        m_gBufferChoice->program()->setUniform<GLint>("normalSource",        2);
        m_gBufferChoice->program()->setUniform<GLint>("worldCoordSource",    3);
        m_gBufferChoice->program()->setUniform<GLint>("depthSource",         4);

        m_camera.setZNear(1.f);
        m_camera.setZFar(16.f);

        m_gBufferChoice->program()->setUniform<GLfloat>("nearZ", m_camera.zNear());
        m_gBufferChoice->program()->setUniform<GLfloat>("farZ",  m_camera.zFar());

        window.addTimer(0, 0, false);

        cameraChanged();
    }

    virtual void framebufferResizeEvent(ResizeEvent & event) override
    {
        glViewport(0, 0, event.width(), event.height());
        m_camera.setViewport(event.width(), event.height());

        m_postprocessing->program()->setUniform<vec2>("screenSize", vec2(event.size()));

        cameraChanged();

        m_colorTexture->image2D(        0, GL_RGBA8,   event.width(), event.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        m_normalTexture->image2D(       0, GL_RGBA16F, event.width(), event.height(), 0, GL_RGBA, GL_FLOAT, nullptr);
        m_geometryTexture->image2D(     0, GL_RGBA16F, event.width(), event.height(), 0, GL_RGBA, GL_FLOAT, nullptr);
        m_depthTexture->image2D(0, GL_DEPTH_COMPONENT, event.width(), event.height(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        m_postprocessedTexture->image2D(0, GL_RGBA8,   event.width(), event.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    void cameraChanged()
    {
        m_sphere->setUniform("transform", m_camera.viewProjection());
        m_sphere->setUniform("modelView", m_camera.view());
        m_sphere->setUniform("normalMatrix", m_camera.normal());
    }

    virtual void paintEvent(PaintEvent & event) override
    {
        WindowEventHandler::paintEvent(event);

        // Sphere Pass

        m_sphereFBO->bind();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_sphere->use();
        m_icosahedron->draw();
        m_sphere->release();

        m_sphereFBO->unbind();

        // Postprocessing Pass

        m_postprocessingFBO->bind();

        glClear(GL_COLOR_BUFFER_BIT);

        m_colorTexture->bindActive(GL_TEXTURE0);
        m_normalTexture->bindActive(GL_TEXTURE1);
        m_geometryTexture->bindActive(GL_TEXTURE2);
        m_depthTexture->bindActive(GL_TEXTURE3);

        m_postprocessing->draw();

        m_colorTexture->unbindActive(GL_TEXTURE0);
        m_normalTexture->unbindActive(GL_TEXTURE1);
        m_geometryTexture->unbindActive(GL_TEXTURE2);
        m_depthTexture->unbindActive(GL_TEXTURE3);

        m_postprocessingFBO->unbind();

        // GBuffer Choice Pass (including blitting)

        // If no FBO is bound to GL_FRAMEBUFFER the default FBO is bound to GL_FRAMEBUFFER

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_postprocessedTexture->bindActive(GL_TEXTURE0);
        m_colorTexture->bindActive(GL_TEXTURE1);
        m_normalTexture->bindActive(GL_TEXTURE2);
        m_geometryTexture->bindActive(GL_TEXTURE3);
        m_depthTexture->bindActive(GL_TEXTURE4);

        m_gBufferChoice->draw();

        m_postprocessedTexture->unbindActive(GL_TEXTURE0);
        m_colorTexture->unbindActive(GL_TEXTURE1);
        m_normalTexture->unbindActive(GL_TEXTURE2);
        m_geometryTexture->unbindActive(GL_TEXTURE3);
        m_depthTexture->unbindActive(GL_TEXTURE4);
    }

    virtual void keyPressEvent(KeyEvent & event) override
    {
        WindowEventHandler::keyPressEvent(event);

        switch (event.key())
        {
        case GLFW_KEY_1:
        case GLFW_KEY_2:
        case GLFW_KEY_3:
        case GLFW_KEY_4:
        case GLFW_KEY_5:
            m_gBufferChoice->program()->setUniform<GLint>("choice", event.key() - 49);
            break;

        case GLFW_KEY_SPACE:
            m_camera.setCenter(vec3( 0.f, 0.f, 0.f));
            m_camera.setEye   (vec3( 0.f, 1.f, 4.f));
            m_camera.setUp    (vec3( 0.f, 1.f, 0.f));
            cameraChanged();
            break;
        }
    }

    virtual void mousePressEvent(MouseEvent & event) override
    {
        switch (event.button())
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            m_nav.panBegin(event.pos());
            event.accept();
            break;

        case GLFW_MOUSE_BUTTON_RIGHT:
            m_nav.rotateBegin(event.pos());
            event.accept();
            break;
        }
    }

    virtual void mouseMoveEvent(MouseEvent & event) override
    {
        switch (m_nav.mode())
        {
        case WorldInHandNavigation::PanInteraction:
            m_nav.panProcess(event.pos());
            event.accept();
            cameraChanged();
            break;

        case WorldInHandNavigation::RotateInteraction:
            m_nav.rotateProcess(event.pos());
            event.accept();
            cameraChanged();
            break;
        case WorldInHandNavigation::NoInteraction:
            break;
        }
    }

    virtual void mouseReleaseEvent(MouseEvent & event) override
    {
        switch (event.button())
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            m_nav.panEnd();
            event.accept();
            break;

        case GLFW_MOUSE_BUTTON_RIGHT:
            m_nav.rotateEnd();
            event.accept();
            break;
        }
    }

    virtual void scrollEvent(ScrollEvent & event) override
    {
        if (WorldInHandNavigation::NoInteraction != m_nav.mode())
            return;

        m_nav.scaleAtMouse(event.pos(), -event.offset().y * 0.1f);
        event.accept();
        cameraChanged();
    }

    virtual float depthAt(const ivec2 & windowCoordinates) const override
    {
        m_sphereFBO->bind();
        float depth = AbstractCoordinateProvider::depthAt(m_camera, GL_DEPTH_COMPONENT, windowCoordinates);
        m_sphereFBO->unbind();

        return depth;
    }

    virtual vec3 objAt(const ivec2 & windowCoordinates) const override
    {
        return unproject(m_camera, static_cast<GLenum>(GL_DEPTH_COMPONENT), windowCoordinates);
    }

    virtual vec3 objAt(const ivec2 & windowCoordinates, const float depth) const override
    {
        return unproject(m_camera, depth, windowCoordinates);
    }

    virtual vec3 objAt(
        const ivec2 & windowCoordinates
    ,   const float depth
    ,   const mat4 & viewProjectionInverted) const override
    {
        return unproject(m_camera, viewProjectionInverted, depth, windowCoordinates);
    }

protected:
    std::unique_ptr<Icosahedron, globjects::HeapOnlyDeleter> m_icosahedron;
    std::unique_ptr<Program, globjects::HeapOnlyDeleter> m_sphere;
    std::unique_ptr<Texture, globjects::HeapOnlyDeleter> m_colorTexture;
    std::unique_ptr<Texture, globjects::HeapOnlyDeleter> m_normalTexture;
    std::unique_ptr<Texture, globjects::HeapOnlyDeleter> m_geometryTexture;
    std::unique_ptr<Texture, globjects::HeapOnlyDeleter> m_depthTexture;
    std::unique_ptr<Framebuffer, globjects::HeapOnlyDeleter> m_sphereFBO;

    std::unique_ptr<ScreenAlignedQuad, globjects::HeapOnlyDeleter> m_postprocessing;
    std::unique_ptr<Texture, globjects::HeapOnlyDeleter> m_postprocessedTexture;
    std::unique_ptr<Framebuffer, globjects::HeapOnlyDeleter> m_postprocessingFBO;

    std::unique_ptr<ScreenAlignedQuad, globjects::HeapOnlyDeleter> m_gBufferChoice;

    Camera m_camera;
    WorldInHandNavigation m_nav;
    ivec2 m_lastMousePos;

    AxisAlignedBoundingBox m_aabb;
};


int main(int /*argc*/, char * /*argv*/[])
{
    info() << "Usage:";
    info() << "\t" << "ESC" << "\t\t"        << "Close example";
    info() << "\t" << "ALT + Enter"          << "\t" << "Toggle fullscreen";
    info() << "\t" << "F11" << "\t\t"        << "Toggle fullscreen";
    info() << "\t" << "F10" << "\t\t"        << "Toggle vertical sync";
    info() << "\t" << "F5" << "\t\t"         << "Reload shaders";
    info() << "\t" << "Space" << "\t\t"      << "Reset camera";
    info() << "\t" << "Left Mouse" << "\t"   << "Pan scene";
    info() << "\t" << "Right Mouse" << "\t"  << "Rotate scene";
    info() << "\t" << "Mouse Wheel" << "\t"  << "Zoom scene";

    info() << "\nSwitch between G-Buffers";
    info() << "\t" << "1" << "\t" << "Postprocessed";
    info() << "\t" << "2" << "\t" << "Color";
    info() << "\t" << "3" << "\t" << "Normal";
    info() << "\t" << "4" << "\t" << "Geometry";
    info() << "\t" << "5" << "\t" << "Depth";

    ContextFormat format;
    format.setVersion(3, 2);
    format.setProfile(ContextFormat::Profile::Core);
    format.setForwardCompatible(true);

    Window::init();

    Window window;
    window.setEventHandler(new EventHandler());

    if (!window.create(format, "GBuffers Example"))
        return 1;

    window.show();

    return MainLoop::run();
}
