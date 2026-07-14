import math
m = 0.938272  # proton mass
K = 13.5
E_beam = m + K
s = 2*m**2 + 2*m*E_beam
print(f"s = {s}")
print(f"sqrt(s) = {math.sqrt(s)}")
