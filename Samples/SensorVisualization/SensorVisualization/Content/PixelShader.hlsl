// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    min16float4 pos   : SV_POSITION;
    min16float3 color : COLOR0;
    min16float2 tex : TEXCOORD0;
};

SamplerState g_samplerState;

Texture2D<min16float4> g_texture0;

// The pixel shader passes through the color data. The color data from 
// is interpolated and assigned to a pixel at the rasterization step.
min16float4 main(PixelShaderInput input) : SV_TARGET
{
    min16float4 sampledColor =
        g_texture0.Sample(
            g_samplerState,
            input.tex);

    return sampledColor;

}
