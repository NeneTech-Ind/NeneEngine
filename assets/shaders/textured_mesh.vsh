cbuffer Constants
{
    float4x4 ModelViewProjection;
    float4 Tint;
};

struct VSInput
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV     : ATTRIB2;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float2 UV    : TEXCOORD0;
    float4 Tint  : COLOR;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    PSIn.Pos = mul(ModelViewProjection, float4(VSIn.Pos, 1.0f));
    PSIn.UV = VSIn.UV;
    PSIn.Tint = Tint;
}
