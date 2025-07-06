ps.1.0

# parallax mapping offset
pom r1, t2, t0, mat:displacement	fmt:half2 precise

# sample diffuse with offset and apply albedo multiplier and lighting
texadd r0, tex0, t0, r1[fmt:half2]
mul r0, r0, v0

# sample glow with offset and take max with diffuse
texadd r1, tex1, t0, r1[fmt:half2]
max r0.xyz, r0, r1

# apply glow multiplier
mul r1, r1, mat:glow
