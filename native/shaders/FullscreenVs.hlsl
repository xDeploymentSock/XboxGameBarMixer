struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput main(uint vertex_id : SV_VertexID)
{
    VertexOutput result;
    result.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    result.position = float4(result.uv.x * 2.0 - 1.0, 1.0 - result.uv.y * 2.0, 0.0, 1.0);
    return result;
}
