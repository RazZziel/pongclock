    const int MaxKernelSize = 25;
    uniform vec2 Offset[MaxKernelSize];
    uniform int KernelSize;
    //uniform vec4 KernelValue[MaxKernelSize];
    uniform vec4 ScaleFactor;
    uniform sampler2D BaseImage;

    void main(void)
    {
        int i;
        vec4 sum = vec4(0.0);
        for ( i=0; i<KernelSize; i++ )
        {
            sum += texture2D( BaseImage,
                              gl_TexCoord[0].st + Offset[i] );// * KernelValue[i];
        }
        //
        gl_FragColor = sum * ScaleFactor;
    }