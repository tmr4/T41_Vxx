## Frequency Translation with Quadrature Oscillator

Reference: Lyons, R.G. (2011): Understanding Digital Processing. – Pearson, 3rd edition, Chapters 8, 13.32

### Quadrature Oscillator

Equations 13-134 and 13-134' from Lyons 13.32:

$$y_{i}(n) = y_{i}(n-1)\cos(\theta) - jy_{q}(n-1)\sin(\theta)$$

$$y_{q}(n) = y_{i}(n-1)\sin(\theta) + jy_{q}(n-1)\cos(\theta)$$

### Digitized Signal

$$x(n) = I(n) + jQ(n)$$

### Translated Signal

$$x'(n) = (I(n) + jQ(n)) \cdot (y_{i}(n) + jy_{q}(n))$$

$$x'(n) = I(n)y_{i}(n) + jQ(n)y_{i}(n) + jI(n)y_{q}(n) + j^2Q(n)y_{q}(n)$$

Simplifying we get:

$$x'(n) = (I(n)y_{i}(n) - Q(n)y_{q}(n)) + j(I(n)y_{q}(n) + Q(n)y_{i}(n))$$

### Resolving Spectral Inversion from High-side Injection Use in T41

With high-side injection, the local oscillator sits *above* the RF signal causing spectral inversion in the QSD (see https://www.rfcafe.com/references/electrical/spectral-inv.htm for some nice images of this). The fine-tuning frequency shift is negated to flip this inversion.

Math to follow!
