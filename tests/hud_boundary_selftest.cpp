#include "core/gfx/gfx_hud.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
extern "C" DWORD WINAPI BO_Identity();
extern "C" __declspec(dllexport) DWORD WINAPI BO_HudBoundarySelftest(ID3D11DeviceContext* ctx,IDXGISwapChain* sc) {
    if(BO_Identity()!=2 || !ctx || !sc) return 1;
    using namespace bvr::gfx_hud;
    ID3D11Device* dev=nullptr; ctx->GetDevice(&dev);
    ID3D11Texture2D* bb=nullptr; ID3D11RenderTargetView* rtv=nullptr;
    ID3D11Texture2D* scene=nullptr; ID3D11ShaderResourceView* srv=nullptr;
    DWORD result=2;
    if(SUCCEEDED(sc->GetBuffer(0,IID_PPV_ARGS(&bb))) && SUCCEEDED(dev->CreateRenderTargetView(bb,nullptr,&rtv))) {
        D3D11_TEXTURE2D_DESC td{}; bb->GetDesc(&td);
        td.Width+=4; td.Height+=6; td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&scene)) && SUCCEEDED(dev->CreateShaderResourceView(scene,nullptr,&srv))) {
            set_armed(true); set_redirect(true); set_triangle_boundary(false);
            on_present(ctx,sc);
            ctx->OMSetRenderTargets(1,&rtv,nullptr); on_setrt(1,&rtv,nullptr);
            D3D11_VIEWPORT vp{0,0,float(td.Width-4),float(td.Height-6),0,1}; ctx->RSSetViewports(1,&vp);
            ctx->PSSetShaderResources(0,1,&srv);
            const auto before=census().boundaries;
            on_draw(ctx,3); bool ok=census().boundaries==before;
            set_triangle_boundary(true);
            on_draw(ctx,6); on_draw_indexed(ctx,3); ok &= census().boundaries==before;
            vp.Width/=2; ctx->RSSetViewports(1,&vp); on_draw(ctx,3); ok &= census().boundaries==before;
            vp.Width*=2; ctx->RSSetViewports(1,&vp);
            auto boundary=on_draw(ctx,3);
            ok &= census().boundaries==before+1 && !boundary.redirectRtv;
            auto ui=on_draw_indexed(ctx,30);
            ok &= ui.redirectRtv!=nullptr && ui.redirectRtv!=rtv;
            ID3D11RenderTargetView* actual=nullptr; ctx->OMGetRenderTargets(1,&actual,nullptr);
            ok &= actual==rtv; if(actual) actual->Release(); // decision never binds/captures scene itself
            set_armed(false); set_redirect(false); result=ok ? 0 : 3;
        }
    }
    ctx->ClearState();
    if(srv) srv->Release(); if(scene) scene->Release(); if(rtv) rtv->Release(); if(bb) bb->Release(); dev->Release();
    return result;
}
