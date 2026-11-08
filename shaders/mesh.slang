struct Transform {
    float4x4 camera;
};

[[vk::push_constant]] ConstantBuffer<Transform> transform;

struct Input {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float4 color : COLOR0;
};

struct Output {
    float4 position : SV_Position;
    [[vk::location(0)]] float4 color : COLOR0;
};

[shader("vertex")]
Output vertex(Input input) {
    Output output;
    output.position = mul(transform.camera, float4(input.position, 1.0));
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragment(Output input) : SV_Target {
    return input.color;
}
