import numpy as np
from scipy.signal import correlate

# ----------------------------
# Parameters
# ----------------------------
IMG_C, IMG_H, IMG_W = 3, 224, 224
K_OUT = 64
K = 3  # 3x3 kernels
PADDING = 1

np.random.seed(1113)
np.set_printoptions(precision=5, suppress=True)

# ----------------------------
# Input image  (C, H, W)
# ----------------------------
input_image = np.random.randn(IMG_C, IMG_H, IMG_W).astype(np.float16)

# ----------------------------
# Kernels  (out_channels, in_channels, kh, kw)
# ----------------------------
kernel = np.random.standard_normal(size=(K_OUT, IMG_C, K, K)).astype(np.float16)

# ----------------------------
# Padding helper
# ----------------------------
def pad2d(tensor, pad):
    if pad == 0:
        return tensor
    C, H, W = tensor.shape
    padded = np.zeros((C, H + 2*pad, W + 2*pad), dtype=tensor.dtype)
    padded[:, pad:pad+H, pad:pad+W] = tensor
    return padded

# ----------------------------
# Conv2D function for (C,H,W)
# ----------------------------
def conv2d(input_tensor, kernel, padding=1):
    Cin, H, W = input_tensor.shape
    Cout, Cin_k, Kh, Kw = kernel.shape
    assert Cin == Cin_k

    input_padded = pad2d(input_tensor, padding)
    out = np.zeros((Cout, H, W), dtype=np.float32)

    for oc in range(Cout):
        # sum over input channels efficiently
        out[oc] = np.sum(
            [correlate(input_padded[ic], kernel[oc, ic], mode='valid')
             for ic in range(Cin)],
            axis=0
        )
    return out.astype(np.float16)

# ----------------------------
# Layer 1: Conv + ReLU
# ----------------------------
out = conv2d(input_image, kernel, PADDING)
out = np.maximum(out, 0)  # ReLU


# ----------------------------
# Save data
# ----------------------------
np.save("input.npy", input_image)
np.save("kernel.npy", kernel)
np.save("output.npy", out)

print("Shapes:")
print("Input :", input_image.shape)
print("Kernel :", kernel.shape)
print("Output :", out.shape)