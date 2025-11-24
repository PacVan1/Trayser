// common.hlsl
// Shared header for testing includes

static const float GarbageConstant = 42.0f;

// A silly function just to test compilation
float GarbageFunction(float x)
{
    // Do something meaningless
    return x * GarbageConstant + 1.0f;
}
