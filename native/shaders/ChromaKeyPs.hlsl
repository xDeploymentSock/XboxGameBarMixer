Texture2DArray<float> luminance_plane : register(t0);
Texture2DArray<float2> chrominance_plane : register(t1);
SamplerState video_sampler : register(s0);

cbuffer Parameters : register(b0)
{
    float4 key_color_tolerance;
    float4 controls; // softness, spill suppression, opacity, input mode
    float4 dimensions_sequence; // pixel width, height, sequence mod 256, key enabled
    float4 yuv_offsets;
    float4 matrix_row0;
    float4 matrix_row1;
    float4 matrix_row2;
    float4 source_rectangle; // normalized origin and visible extent in the allocation
};

float3 diagnostic(float2 uv)
{
    float3 rgb = key_color_tolerance.rgb;
    float2 pixel = uv * dimensions_sequence.xy;
    float2 edge = min(pixel, dimensions_sequence.xy - pixel);
    float2 box = abs(uv - 0.5);
    float marker_x = 0.1 + 0.8 * dimensions_sequence.z / 255.0;
    if (abs(uv.x - marker_x) < 0.012 && abs(uv.y - 0.15) < 0.02) rgb = float3(1.0, 0.1, 0.1);
    if (abs(pixel.y - dimensions_sequence.y * 0.5) < 1.0 ||
        abs(pixel.x - dimensions_sequence.x * 0.5) < 1.0) rgb = float3(1.0, 1.0, 1.0);
    if ((abs(box.x - 0.3) < 0.002 && box.y <= 0.3) ||
        (abs(box.y - 0.3) < 0.002 && box.x <= 0.3)) rgb = float3(0.0, 1.0, 1.0);
    if (min(edge.x, edge.y) < 3.0) rgb = float3(1.0, 1.0, 1.0);
    return rgb;
}

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float3 rgb;
    if (controls.w < 0.5)
    {
        rgb = diagnostic(uv);
    }
    else
    {
        uint texture_width, texture_height, texture_slices;
        luminance_plane.GetDimensions(texture_width, texture_height, texture_slices);
        float2 texel = 1.0 / float2(texture_width, texture_height);
        float2 source_uv = source_rectangle.xy + uv * source_rectangle.zw;
        // Clamp each plane to the visible region so linear filtering never
        // blends hardware padding into the edge of the HUD.
        float2 luma_uv = clamp(source_uv, source_rectangle.xy + 0.5 * texel,
                              source_rectangle.xy + source_rectangle.zw - 0.5 * texel);
        float2 chroma_uv = clamp(source_uv, source_rectangle.xy + texel,
                                source_rectangle.xy + source_rectangle.zw - texel);
        float3 yuv = float3(luminance_plane.Sample(video_sampler, float3(luma_uv, 0.0)),
                           chrominance_plane.Sample(video_sampler, float3(chroma_uv, 0.0)));
        yuv -= yuv_offsets.xyz;
        rgb = saturate(float3(dot(matrix_row0.xyz, yuv),
                              dot(matrix_row1.xyz, yuv), dot(matrix_row2.xyz, yuv)));
    }
    float alpha = 1.0;
    if (dimensions_sequence.w > 0.5)
    {
        float distance_from_key = distance(rgb, key_color_tolerance.rgb);
        float lower = key_color_tolerance.w;
        // step avoids undefined smoothstep behavior when both edges are equal.
        alpha = controls.x > 0.0
            ? smoothstep(lower, lower + controls.x, distance_from_key)
            : step(lower + 0.00001, distance_from_key);

        float3 key_chroma = key_color_tolerance.rgb -
            min(key_color_tolerance.r, min(key_color_tolerance.g, key_color_tolerance.b));
        float chroma_length = dot(key_chroma, key_chroma);
        if (chroma_length > 0.00001)
        {
            float neutral = min(rgb.r, min(rgb.g, rgb.b));
            float spill = max(0.0, dot(rgb - neutral, key_chroma) / chroma_length);
            rgb = saturate(rgb - key_chroma * spill * controls.y * (1.0 - alpha));
        }
    }
    alpha *= controls.z;
    // Game Bar composition receives premultiplied BGRA, including zero RGB at alpha 0.
    return float4(rgb * alpha, alpha);
}
