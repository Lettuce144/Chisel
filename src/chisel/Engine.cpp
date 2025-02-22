
#include "Engine.h"
#include "common/Time.h"
#include "chisel/Gizmos.h"
#include "chisel/Handles.h"
#include "chisel/Selection.h"
#include "gui/Common.h"
#include "assets/Assets.h"
#include "core/Primitives.h"

#include <bit>

namespace chisel
{

    static render::RenderContext& rctx = Engine.rctx;
    static render::RenderContext& r = Engine.rctx;

    void Engine::Init()
    {
        // Create window
        window->Create("Chisel", 1920, 1080, true, false);

        // Initialize render system
        rctx.Init(window);

        // Setup gizmos and handles
        Primitives.Init();
        Gizmos.Init();
        Handles.Init();

        // Load editor shaders
        cs_ObjectID = render::ComputeShader(r.device.ptr(), "objectid");
        cs_ObjectID.buffers.push_back(r.CreateCSInputBuffer<uint2>());
        cs_ObjectID.buffers.push_back(r.CreateCSOutputBuffer<uint>());
        cs_ObjectID.buffers[1].AddStagingBuffer(r.device.ptr());
    }

    void Engine::Loop()
    {
        systems.Start();

        Time::Seconds lastTime    = Time::GetTime();
        Time::Seconds accumulator = 0;

        while (!window->ShouldClose())
        {
            auto currentTime = Time::GetTime();
            auto deltaTime   = currentTime - lastTime;
            lastTime = currentTime;

            Time.Advance(deltaTime);

            // TODO: Whether we use deltaTime or unscaled.deltaTime affects
            // whether fixed.deltaTime needs to be changed with timeScale.
            // Perhaps the accumulator logic could go into Time.
            accumulator += Time.deltaTime;

            while (accumulator >= Time.fixed.deltaTime)
            {
                // Perform fixed updates
                systems.Tick();

                accumulator     -= Time.fixed.deltaTime;
                Time.fixed.time += Time.fixed.deltaTime;
                Time.tickCount++;
            }

            // Amount to lerp between physics steps
            [[maybe_unused]] double alpha = accumulator / Time.fixed.deltaTime;

            // Clear buffered input
            Input.Update();

            // Process input
            window->PreUpdate();

            // Setup to render
            rctx.BeginFrame();

            // Perform system updates
            systems.Update();

            // Finish rendering
            rctx.EndFrame();
            OnEndFrame(rctx);
            
            // Present to non-main windows
            GUI::Present();

            // Present to main window
            window->Update();

            Time.frameCount++;
        }
    }

    void Engine::Shutdown()
    {
        window->OnDetach();
        rctx.Shutdown();
        delete window;
        Window::Shutdown();
    }

    void Engine::PickObject(uint2 mouse, const Rc<render::RenderTarget>& rt_ObjectID, void callback(void*))
    {
        auto bufferIn = cs_ObjectID.buffers[0];

        // Update input buffer
        r.UpdateDynamicBuffer(bufferIn.buffer.ptr(), mouse);

        // When rendering completes...
        OnEndFrame.Once([rt_ObjectID, callback](render::RenderContext& r)
        {
            // Unbind render targets
            r.ctx->OMSetRenderTargets(0, nullptr, nullptr);

            extern class Engine Engine;
            auto bufferIn = Engine.cs_ObjectID.buffers[0];
            auto bufferOut = Engine.cs_ObjectID.buffers[1];

            // Bind the render target, input coords, output value
            ID3D11ShaderResourceView* srvs[] = { rt_ObjectID->srvLinear.ptr(), bufferIn.srv.ptr() };
            r.ctx->CSSetShaderResources(0, 2, srvs);
            r.ctx->CSSetUnorderedAccessViews(0, 1, &bufferOut.uav, nullptr);

            // Run the compute shader
            r.ctx->CSSetShader(Engine.cs_ObjectID.cs.ptr(), nullptr, 0);
            r.ctx->Dispatch(1, 1, 1);

            // Download the output value
            bufferOut.Download(callback);
        });
    }
}
