import sys

def main():
    files = [
        "config/hist/hist_auau13p5_anaLambdaNuclearId.yaml",
        "config/hist/hist_auau19_anaLambdaNuclearId.yaml",
        "config/hist/hist_auau3p85_anaLambdaNuclearId.yaml"
    ]

    parts = ["d", "t", "3He", "4He"]

    for f in files:
        with open(f, "a") as out:
            for p in parts:
                for i in range(9):
                    # True
                    out.write(f"  hKstarMass_{p}_CentBin{i}:\n")
                    out.write(f"    title: 'k* vs Lambda InvMass ({p}, Cent {i})'\n")
                    out.write(f"    axes: ['Kstar', 'InvMassLambda']\n")
                    # Mixed
                    out.write(f"  hKstarMass_Mixed_{p}_CentBin{i}:\n")
                    out.write(f"    title: 'Mixed k* vs Lambda InvMass ({p}, Cent {i})'\n")
                    out.write(f"    axes: ['Kstar', 'InvMassLambda']\n")

if __name__ == "__main__":
    main()
