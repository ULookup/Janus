#include "Backend/OpenGL/OpenGLRenderDevice.h"

#include "Core/Log/Log.h"

#include <SDL3/SDL_video.h>

#include <glad/gl.h>

#include <cstdint>
#include <memory>
#include <string>

namespace Janus
{

namespace
{

constexpr const char* VertexShaderSource = R"(
#version 450 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform mat4 uViewProjection;
out vec2 vUV;
out vec4 vColor;
void main()
{
    gl_Position = uViewProjection * vec4(aPosition, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
)";

constexpr const char* FragmentShaderSource = R"(
#version 450 core
uniform sampler2D uTexture;
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;
void main()
{
    FragColor = texture(uTexture, vUV) * vColor;
}
)";

#if defined(JANUS_DEBUG)

void GLAPIENTRY OpenGLDebugCallback(
    GLenum,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei,
    const GLchar* message,
    const void*)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        return;
    }

    const char* text = message == nullptr ? "<no message>" : message;

    if (severity == GL_DEBUG_SEVERITY_HIGH)
    {
        JANUS_CORE_ERROR(
            "[OpenGL] debug message id={} type=0x{:X}: {}",
            id,
            static_cast<u32>(type),
            text);
        return;
    }

    if (severity == GL_DEBUG_SEVERITY_MEDIUM)
    {
        JANUS_CORE_WARN(
            "[OpenGL] debug message id={} type=0x{:X}: {}",
            id,
            static_cast<u32>(type),
            text);
        return;
    }

    JANUS_CORE_TRACE(
        "[OpenGL] debug message id={} type=0x{:X}: {}",
        id,
        static_cast<u32>(type),
        text);
}

void EnableOpenGLDebugOutput()
{
    GLint contextFlags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &contextFlags);

    if ((contextFlags & GL_CONTEXT_FLAG_DEBUG_BIT) == 0)
    {
        JANUS_CORE_WARN(
            "OpenGL debug output is unavailable because the current context "
            "was not created with the debug flag.");
        return;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(OpenGLDebugCallback, nullptr);
    glDebugMessageControl(
        GL_DONT_CARE,
        GL_DONT_CARE,
        GL_DEBUG_SEVERITY_NOTIFICATION,
        0,
        nullptr,
        GL_FALSE);
}

#endif

[[nodiscard]] u32 CompileShader(
    u32 type,
    const char* source)
{
    const u32 shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success == 0)
    {
        char infoLog[512]{};
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        JANUS_CORE_ERROR("Shader compile error: {}", infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

[[nodiscard]] Result<u32> LinkProgram()
{
    const u32 vertexShader =
        CompileShader(GL_VERTEX_SHADER, VertexShaderSource);
    const u32 fragmentShader =
        CompileShader(GL_FRAGMENT_SHADER, FragmentShaderSource);

    if (vertexShader == 0 || fragmentShader == 0)
    {
        if (vertexShader != 0)
        {
            glDeleteShader(vertexShader);
        }

        if (fragmentShader != 0)
        {
            glDeleteShader(fragmentShader);
        }

        return Result<u32>::Failure(
            ErrorCode::ShaderCompileFailed,
            "Failed to compile the built-in renderer shader.");
    }

    const u32 program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (success == 0)
    {
        char infoLog[512]{};
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        glDeleteProgram(program);
        return Result<u32>::Failure(
            ErrorCode::ShaderCompileFailed,
            std::string("Failed to link the built-in renderer shader: ")
                + infoLog);
    }

    return Result<u32>::Success(program);
}

} // namespace

Result<std::unique_ptr<OpenGLRenderDevice>>
    OpenGLRenderDevice::Create()
{
    if (!gladLoadGL(
            reinterpret_cast<GLADloadfunc>(
                SDL_GL_GetProcAddress)))
    {
        return Result<std::unique_ptr<OpenGLRenderDevice>>::Failure(
            ErrorCode::RendererInitFailed,
            "Failed to load OpenGL functions with glad.");
    }

    auto device = std::unique_ptr<OpenGLRenderDevice>(
        new OpenGLRenderDevice());

#if defined(JANUS_DEBUG)
    EnableOpenGLDebugOutput();
#endif

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const auto programResult = LinkProgram();

    if (!programResult)
    {
        return Result<std::unique_ptr<OpenGLRenderDevice>>::Failure(
            programResult.GetError());
    }

    device->m_Shaders[1] = programResult.Value();
    device->m_NextHandle = 2;
    device->m_CurrentShader = ShaderHandle{1};

    return Result<std::unique_ptr<OpenGLRenderDevice>>::Success(
        std::move(device));
}

OpenGLRenderDevice::~OpenGLRenderDevice()
{
    for (const auto& [handle, object] : m_VertexBuffers)
    {
        glDeleteBuffers(1, &object);
    }

    for (const auto& [handle, object] : m_IndexBuffers)
    {
        glDeleteBuffers(1, &object);
    }

    for (const auto& [handle, object] : m_VertexArrays)
    {
        glDeleteVertexArrays(1, &object);
    }

    for (const auto& [handle, object] : m_Shaders)
    {
        glDeleteProgram(object);
    }

    for (const auto& [handle, object] : m_Textures)
    {
        glDeleteTextures(1, &object);
    }

    for (const auto& [handle, object] : m_Framebuffers)
    {
        glDeleteFramebuffers(1, &object);
    }
}

Result<VertexBufferHandle> OpenGLRenderDevice::CreateVertexBuffer(
    const BufferDesc& desc)
{
    u32 object = 0;
    glCreateBuffers(1, &object);
    glNamedBufferData(
        object,
        static_cast<GLsizeiptr>(desc.size),
        desc.data,
        GL_STREAM_DRAW);

    const u32 handle = m_NextHandle++;
    m_VertexBuffers[handle] = object;

    return Result<VertexBufferHandle>::Success(
        VertexBufferHandle{handle});
}

void OpenGLRenderDevice::DestroyVertexBuffer(
    VertexBufferHandle handle)
{
    const auto iterator = m_VertexBuffers.find(handle.value);

    if (iterator == m_VertexBuffers.end())
    {
        return;
    }

    glDeleteBuffers(1, &iterator->second);
    m_VertexBuffers.erase(iterator);
}

Result<IndexBufferHandle> OpenGLRenderDevice::CreateIndexBuffer(
    const BufferDesc& desc)
{
    // GL_ELEMENT_ARRAY_BUFFER binding belongs to VAO state in core profile.
    // Upload storage directly to the buffer object so creation is valid even
    // when no VAO is bound.
    u32 object = 0;
    glCreateBuffers(1, &object);
    glNamedBufferData(
        object,
        static_cast<GLsizeiptr>(desc.size),
        desc.data,
        GL_STREAM_DRAW);

    const u32 handle = m_NextHandle++;
    m_IndexBuffers[handle] = object;

    return Result<IndexBufferHandle>::Success(
        IndexBufferHandle{handle});
}

void OpenGLRenderDevice::DestroyIndexBuffer(
    IndexBufferHandle handle)
{
    const auto iterator = m_IndexBuffers.find(handle.value);

    if (iterator == m_IndexBuffers.end())
    {
        return;
    }

    glDeleteBuffers(1, &iterator->second);
    m_IndexBuffers.erase(iterator);
}

Result<VertexArrayHandle> OpenGLRenderDevice::CreateVertexArray(
    const VertexLayout& layout,
    VertexBufferHandle vertexBuffer)
{
    const auto bufferIterator =
        m_VertexBuffers.find(vertexBuffer.value);

    if (bufferIterator == m_VertexBuffers.end())
    {
        return Result<VertexArrayHandle>::Failure(
            ErrorCode::InvalidArgument,
            "Cannot create a vertex array from an unknown vertex buffer.");
    }

    u32 object = 0;
    glGenVertexArrays(1, &object);
    glBindVertexArray(object);
    glBindBuffer(GL_ARRAY_BUFFER, bufferIterator->second);

    for (const VertexAttribute& attribute : layout.attributes)
    {
        const int componentCount =
            attribute.type == VertexAttributeType::Float2 ? 2 : 4;

        glEnableVertexAttribArray(attribute.location);
        glVertexAttribPointer(
            attribute.location,
            componentCount,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(layout.stride),
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(attribute.offset)));
    }

    glBindVertexArray(0);

    const u32 handle = m_NextHandle++;
    m_VertexArrays[handle] = object;

    return Result<VertexArrayHandle>::Success(
        VertexArrayHandle{handle});
}

void OpenGLRenderDevice::DestroyVertexArray(
    VertexArrayHandle handle)
{
    const auto iterator = m_VertexArrays.find(handle.value);

    if (iterator == m_VertexArrays.end())
    {
        return;
    }

    glDeleteVertexArrays(1, &iterator->second);
    m_VertexArrays.erase(iterator);
}

Result<ShaderHandle> OpenGLRenderDevice::CreateShader(
    const ShaderDesc&)
{
    return Result<ShaderHandle>::Failure(
        ErrorCode::InvalidArgument,
        "The OpenGL renderer uses a single built-in shader.");
}

void OpenGLRenderDevice::DestroyShader(ShaderHandle handle)
{
    const auto iterator = m_Shaders.find(handle.value);

    if (iterator == m_Shaders.end())
    {
        return;
    }

    glDeleteProgram(iterator->second);
    m_Shaders.erase(iterator);
}

Result<TextureHandle> OpenGLRenderDevice::CreateTexture(
    const TextureDesc& desc)
{
    constexpr usize channels = 4;

    const usize requiredDataSize =
        static_cast<usize>(desc.width)
        * static_cast<usize>(desc.height)
        * channels;

    if (desc.width == 0
        || desc.height == 0
        || (desc.data != nullptr && desc.dataSize < requiredDataSize)
        || (desc.data == nullptr && desc.dataSize != 0))
    {
        return Result<TextureHandle>::Failure(
            ErrorCode::TextureCreateFailed,
            "Texture dimensions and optional RGBA8 data size are invalid.");
    }

    u32 object = 0;
    glGenTextures(1, &object);
    glBindTexture(GL_TEXTURE_2D, object);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        static_cast<GLsizei>(desc.width),
        static_cast<GLsizei>(desc.height),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        desc.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    const u32 handle = m_NextHandle++;
    m_Textures[handle] = object;

    return Result<TextureHandle>::Success(
        TextureHandle{handle});
}

void OpenGLRenderDevice::DestroyTexture(
    TextureHandle handle)
{
    const auto iterator = m_Textures.find(handle.value);

    if (iterator == m_Textures.end())
    {
        return;
    }

    glDeleteTextures(1, &iterator->second);
    m_Textures.erase(iterator);
}

Result<FramebufferHandle> OpenGLRenderDevice::CreateFramebuffer(
    const FramebufferDesc& desc)
{
    const auto textureIterator =
        m_Textures.find(desc.colorTexture.value);

    if (textureIterator == m_Textures.end())
    {
        return Result<FramebufferHandle>::Failure(
            ErrorCode::FramebufferCreateFailed,
            "Framebuffer color texture does not exist.");
    }

    u32 object = 0;
    glGenFramebuffers(1, &object);
    glBindFramebuffer(GL_FRAMEBUFFER, object);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        textureIterator->second,
        0);

    const u32 status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        glDeleteFramebuffers(1, &object);
        return Result<FramebufferHandle>::Failure(
            ErrorCode::FramebufferCreateFailed,
            "Framebuffer is incomplete.");
    }

    const u32 handle = m_NextHandle++;
    m_Framebuffers[handle] = object;

    return Result<FramebufferHandle>::Success(
        FramebufferHandle{handle});
}

void OpenGLRenderDevice::DestroyFramebuffer(
    FramebufferHandle handle)
{
    const auto iterator = m_Framebuffers.find(handle.value);

    if (iterator == m_Framebuffers.end())
    {
        return;
    }

    glDeleteFramebuffers(1, &iterator->second);
    m_Framebuffers.erase(iterator);
}

Result<void> OpenGLRenderDevice::BindFramebuffer(
    FramebufferHandle handle)
{
    const auto iterator = m_Framebuffers.find(handle.value);
    if (iterator == m_Framebuffers.end())
    {
        return Result<void>::Failure(
            ErrorCode::InvalidArgument,
            "Cannot bind an unknown framebuffer.");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, iterator->second);
    return Result<void>::Success();
}

void OpenGLRenderDevice::BindDefaultFramebuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderDevice::SetViewport(Viewport viewport)
{
    m_Viewport = viewport;
    glViewport(
        0,
        0,
        static_cast<GLsizei>(viewport.width),
        static_cast<GLsizei>(viewport.height));
}

void OpenGLRenderDevice::SetViewProjection(const Mat4& matrix)
{
    m_ViewProjection = matrix;
}

void OpenGLRenderDevice::UseShader(ShaderHandle handle)
{
    const auto iterator = m_Shaders.find(handle.value);

    if (iterator == m_Shaders.end())
    {
        return;
    }

    m_CurrentShader = handle;
    glUseProgram(iterator->second);

    const int viewProjectionLocation =
        glGetUniformLocation(iterator->second, "uViewProjection");

    if (viewProjectionLocation != -1)
    {
        glUniformMatrix4fv(
            viewProjectionLocation,
            1,
            GL_FALSE,
            m_ViewProjection.Data());
    }
}

void OpenGLRenderDevice::Clear(Color color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderDevice::DrawIndexed(
    const DrawCommand& command)
{
    const auto vertexArrayIterator =
        m_VertexArrays.find(command.vertexArray.value);
    const auto indexBufferIterator =
        m_IndexBuffers.find(command.indexBuffer.value);
    const auto textureIterator =
        m_Textures.find(command.texture.value);

    if (vertexArrayIterator == m_VertexArrays.end() ||
        indexBufferIterator == m_IndexBuffers.end())
    {
        JANUS_CORE_ERROR("DrawIndexed received an unknown buffer handle.");
        return;
    }

    glBindVertexArray(vertexArrayIterator->second);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferIterator->second);

    if (textureIterator != m_Textures.end())
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureIterator->second);
    }

    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(command.indexCount),
        GL_UNSIGNED_INT,
        nullptr);

    glBindVertexArray(0);
}

} // namespace Janus
