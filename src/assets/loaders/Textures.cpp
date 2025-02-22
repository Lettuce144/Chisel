
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

#include "assets/Assets.h"
#include "render/Render.h"
#include "render/TextureFormat.h"
#include "common/Bit.h"
#include "chisel/Engine.h"
#include "libvtf-plusplus/libvtf++.hpp"

#include <span>

namespace chisel
{
    static void LoadTexture(Texture& tex, const Buffer& data)
    {
        int width, height, channels;

        // 8 bits per channel
        std::unique_ptr<uint8_t[]> owned_data;
        owned_data.reset(stbi_load_from_memory(data.data(), int(data.size()), &width, &height, &channels, STBI_rgb_alpha));

        if (!owned_data)
            throw std::runtime_error("STB failed to load texture.");

        D3D11_TEXTURE2D_DESC desc =
        {
            .Width = UINT(width),
            .Height = UINT(height),
            .MipLevels = 1,
            .ArraySize = 1,
            .Format = DXGI_FORMAT_R8G8B8A8_TYPELESS,
            .SampleDesc = { 1, 0 },
            .Usage = D3D11_USAGE_IMMUTABLE,
            .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        };
        D3D11_SUBRESOURCE_DATA initialData =
        {
            .pSysMem = owned_data.get(),
            .SysMemPitch = UINT(width) * 4u,
            .SysMemSlicePitch = 0,
        };
        Engine.rctx.device->CreateTexture2D(&desc, &initialData, &tex.texture);
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDescLinear =
        {
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
            .Texture2D =
            {
                .MostDetailedMip = 0,
                .MipLevels = UINT(-1),
            },
        };
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDescSRGB =
        {
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
            .Texture2D =
            {
                .MostDetailedMip = 0,
                .MipLevels = UINT(-1),
            },
        };
        Engine.rctx.device->CreateShaderResourceView(tex.texture.ptr(), &srvDescLinear, &tex.srvLinear);
        Engine.rctx.device->CreateShaderResourceView(tex.texture.ptr(), &srvDescSRGB, &tex.srvSRGB);
    }

    static AssetLoader<Texture, FixedString(".PNG")> PNGLoader = &LoadTexture;
    static AssetLoader<Texture, FixedString(".TGA")> TGALoader = &LoadTexture;

    inline DXGI_FORMAT RemapVTFImageFormat(libvtf::ImageFormat format)
    {
        switch (format)
        {
        case libvtf::ImageFormats::RGBA8888:       return DXGI_FORMAT_R8G8B8A8_UNORM;
        case libvtf::ImageFormats::BGRA8888:       return DXGI_FORMAT_B8G8R8A8_UNORM;
        case libvtf::ImageFormats::BGR565:         return DXGI_FORMAT_B5G6R5_UNORM;
        case libvtf::ImageFormats::DXT1_RUNTIME: [[fallthrough]];
        case libvtf::ImageFormats::DXT1:           return DXGI_FORMAT_BC1_UNORM;
        case libvtf::ImageFormats::DXT3:           return DXGI_FORMAT_BC2_UNORM;
        case libvtf::ImageFormats::DXT5:           return DXGI_FORMAT_BC3_UNORM;
        case libvtf::ImageFormats::R32F:           return DXGI_FORMAT_R32_FLOAT;
        case libvtf::ImageFormats::RG3232F:        return DXGI_FORMAT_R32G32_FLOAT;
        case libvtf::ImageFormats::RGBA32323232F:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default: throw std::runtime_error("Cannot remap format!");
        }
    }

    static AssetLoader<Texture, FixedString(".VTF")> VTFLoader = [](Texture& tex, const Buffer& data)
    {
        // TODO: Make copy-less.
        libvtf::VTFData vtfData(data);

        const auto& header = vtfData.getHeader();

        DXGI_FORMAT format = RemapVTFImageFormat(header.format);

        D3D11_TEXTURE2D_DESC desc =
        {
            .Width      = header.width,
            .Height     = header.height,
            .MipLevels  = header.numMipLevels,
            .ArraySize  = 1,//header.depth,
            .Format     = LinearToTypeless(format),
            .SampleDesc = { 1, 0 },
            .Usage      = D3D11_USAGE_IMMUTABLE,
            .BindFlags  = D3D11_BIND_SHADER_RESOURCE,
        };
        std::vector<D3D11_SUBRESOURCE_DATA> mipData;
        for (uint8_t i = 0; i < header.numMipLevels; i++)
        {
            const uint32_t blockSize = GetBlockSize(desc.Format).first;

            std::span<const uint8_t> data = vtfData.imageData(0, 0, i);

            auto [width, _, __] = libvtf::adjustImageSizeByMip(header.width, header.height, 1u, i);
            width = align(width, blockSize);

            D3D11_SUBRESOURCE_DATA initialData =
            {
                .pSysMem          = data.data(),
                .SysMemPitch      = (width / blockSize) * GetElementSize(desc.Format),
                .SysMemSlicePitch = 0,
            };
            mipData.push_back(initialData);
        }
        Engine.rctx.device->CreateTexture2D(&desc, mipData.data(), &tex.texture);
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDescLinear =
        {
            .Format = format,
            .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
            .Texture2D =
            {
                .MostDetailedMip = 0,
                .MipLevels = UINT(-1),
            },
        };
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDescSRGB =
        {
            .Format = LinearToSRGB(format),
            .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
            .Texture2D =
            {
                .MostDetailedMip = 0,
                .MipLevels = UINT(-1),
            },
        };
        Engine.rctx.device->CreateShaderResourceView(tex.texture.ptr(), &srvDescLinear, &tex.srvLinear);
        Engine.rctx.device->CreateShaderResourceView(tex.texture.ptr(), &srvDescSRGB, &tex.srvSRGB);
    };
}
