class PIFO:
    def __init__(self, id, alg, children):
        self.pifoid = id
        self.schedule_algorithm = alg
        self.children_node = children

class ScheduleAlgo:
    def __init__(self, treepath, pifo_map):
        self.treepath = treepath
        self.pifo_map = pifo_map

    def SaveToFile(self, filename):
        Treepath = self.treepath
        pifo_map = self.pifo_map

        with open(filename, "w") as outfile:
            outfile.write("#PIFO\n")
            for pifoid, pifo_node in pifo_map.items():
                outfile.write(f"{pifoid} {pifo_node.schedule_algorithm}")
                for child in pifo_node.children_node:
                    child_id, is_pifo, parameter = child
                    outfile.write(f" {{{child_id}, {'true' if is_pifo else 'false'}, {parameter}}}")
                outfile.write("\n")

            outfile.write("\n#Flow\n")
            for flowid, treepath in Treepath.items():
                outfile.write(f"{flowid} {{{', '.join(map(str, treepath))}}}\n")
