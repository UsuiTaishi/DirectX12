cbuffer b0 : register(b0)
{
    matrix Mat;
};

float4 vsMain( float4 pos : POSITION ) : SV_POSITION
{
    return mul(Mat, pos);
} 