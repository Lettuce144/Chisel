
#include "chisel/Chisel.h"
#include "chisel/MapRender.h"
#include "common/String.h"
#include "chisel/VMF/KeyValues.h"
#include "chisel/VMF/VMF.h"
#include "chisel/FGD/FGD.h"

#include "console/Console.h"
#include "gui/ConsoleWindow.h"
#include "gui/Layout.h"
#include "gui/Inspector.h"
#include "gui/Viewport.h"
#include "gui/Keybinds.h"

#include "common/Filesystem.h"
#include "render/Render.h"

#include <cstring>
#include <vector>

namespace chisel
{
    void Chisel::Run()
    {
        Tools.Init();

        fgd = new FGD("core/test.fgd");

        // Add chisel systems...
        Renderer = &Tools.systems.AddSystem<MapRender>();
        Tools.systems.AddSystem<Keybinds>();
        Tools.systems.AddSystem<Layout>();
        Tools.systems.AddSystem<MainToolbar>();
        Tools.systems.AddSystem<SelectionModeToolbar>();
        Tools.systems.AddSystem<Inspector>();
        viewport = &Tools.systems.AddSystem<Viewport>();

        // Setup Object ID pass
#if 0
        Tools.Renderer.OnEndCamera += [](render::RenderContext& ctx)
        {
            extern class chisel::Chisel Chisel;
            
            Tools.BeginSelectionPass(ctx);
            Chisel.Renderer->DrawSelectionPass();
        };
#endif

        Tools.Loop();
        Tools.Shutdown();
    }

    Chisel::~Chisel()
    {
        delete fgd;
    }

    void Chisel::CloseMap()
    {
        Selection.Clear();
        map.Clear();
    }
    
    bool Chisel::LoadVMF(std::string_view path)
    {
        auto text = fs::readTextFile(path);
        if (!text)
            return false;
        
        auto kv = KeyValues::Parse(*text);
        if (!kv)
            return false;

        VMF::VMF vmf(*kv);
        vmf.Import(map);

        return true;
    }

    void Chisel::CreateEntityGallery()
    {
        PointEntity* obsolete = map.AddPointEntity("obsolete");
        obsolete->origin = vec3(-8, -8, 0);

        vec3 origin = vec3(-7, -8, 0);
        for (auto& [name, cls] : fgd->classes)
        {
            if (cls.type == FGD::SolidClass || cls.type == FGD::BaseClass)
                continue;

            PointEntity* ent = map.AddPointEntity(name.c_str());
            ent->origin = origin * 128.f;
            if (++origin.x > 8)
            {
                origin.x = -8;
                origin.y++;
            }
        }
    }
}

namespace chisel::commands
{
    static ConCommand quit("quit", "Quit the application", []() {
        Tools.Shutdown();
        exit(0);
    });
    
    static ConCommand open_vmf("open_vmf", "Load a VMF from a file path.", [](ConCmd& cmd)
    {
        if (cmd.argc != 1)
            return Console.Error("Usage: open_vmf <path>");

        if (!Chisel.LoadVMF(cmd.argv[0]))
            Console.Error("Failed to load VMF '{}'", cmd.argv[0]);
    });
}

int main(int argc, char* argv[])
{
    using namespace chisel;
    Chisel.Run();
}
