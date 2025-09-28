# RHI Layer design

RHI
├── Render-Resource
│   ├── Buffer
│   ├── Texture
│   ├── Sampler
│   └── Shader
│
├── Pipeline-State
│   ├── PipelineLayout
│   ├── GraphicsPipeline
│   └── ComputePipeline
│
├── Command-Execution
│   ├── CommandBuffer
│   └── CommandQueue
│
├── Synchronization
│   ├── Fence
│   ├── Semaphore
│   └── Barrier
│
├── Render-Pass
│   ├── RenderPass
│   ├── Framebuffer
│   └── Swapchain
│
└── Core
    └── Device
