ps.1.0

# aliases
alias diff, r0
alias glow, r1
alias spec, r1

# sample diffuse and multiply by diffuse lighting
tex diff, tex0, t0 # sample diffuse
mul diff, diff, v0 # multiply diffuse color with diffuse light

# sample specular and multiply with specular light and add to diffuse result
#tex spec, tex3, t0
#mad diff.rgb, spec.rgb, v1.rgb, diff.rgb

# sample glow and take max with diffuse
tex glow, tex1, t0
max diff.rgb, diff.rgb, glow.rgb

# apply glow multiplier
mul glow, glow, mat:glow
