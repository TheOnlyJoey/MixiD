#!/usr/bin/env python3

import os, sys


smartlist = ""
# -----------------------------------------------------

def res2c(namespace, filenames):
	global smartlist
	fdH = open("%s.hpp" % namespace, "w")
	fdH.write("/* (Auto-generated binary data file). */\n")
	fdH.write("\n")
	fdH.write("#ifndef BINARY_%s_HPP\n" % namespace.upper())
	fdH.write("#define BINARY_%s_HPP\n" % namespace.upper())
	fdH.write("\n")
	#fdH.write("namespace %s\n" % namespace)
	#fdH.write("{\n")

	fdC = open("%s.cpp" % namespace, "w")
	fdC.write("/* (Auto-generated binary data file). */\n")
	fdC.write("\n")
	fdC.write("#include \"%s.hpp\"\n" % namespace)
	#fdC.write("typedef std::basic_string <unsigned char> ustring;")
	fdC.write("\n")

	tempIndex = 1

	for filename in filenames:
		shortFilename = filename.rsplit(os.sep, 1)[-1].split(".", 1)[0]
		fileType = filename.rsplit(os.sep, 1)[-1].split(".", 1)[1]
		rawFilename = shortFilename
		shortFilename = shortFilename.replace("-", "_").replace("@","_")

		resData = open(filename, 'rb').read()

		varName = (shortFilename.upper() + "_" + fileType.upper())
		print("Generating data for \"%s\"" % (filename))
		print("With filetype \"%s\"" % (fileType))

		fdH.write("    extern const unsigned char* %s;\n" % varName)
		fdH.write("    const unsigned int %sDataSize = %i;\n" % (shortFilename, len(resData)))
		
		#optional: generate smart lookup list
		smartlist += '{"' + rawFilename+"."+fileType + '", ustring(' + varName + ', ' + str(len(resData)) + ')}'
		if tempIndex != len(filenames):
			smartlist += ","

		if tempIndex != len(filenames):
			fdH.write("\n")

		fdC.write("static const unsigned char %s_%i[] = {\n" % (varName, tempIndex))

		curColumn = 1
		fdC.write(" ")

		for data in resData:
			if curColumn == 0:
				fdC.write(" ")

			fdC.write(" %3u," % data)

			if curColumn > 20:
				fdC.write("\n ")
				curColumn = 1
			else:
				curColumn += 1

		fdC.write("};\n")
		fdC.write("const unsigned char* %s = (const unsigned char*)%s_%i;\n" % (varName, varName, tempIndex))

		if tempIndex != len(filenames):
			fdC.write("\n")

		tempIndex += 1

	#fdH.write("}\n")
	fdH.write("\n")
	fdH.write("#endif // BINARY_%s_HPP\n" % namespace.upper())
	fdH.write("\n")
	fdH.close()

	fdC.write("\n")
	fdC.close()

# -----------------------------------------------------

if __name__ == '__main__':
	if len(sys.argv) not in (3, 4):
		print("Usage: %s <namespace> <resource-folder> [output-folder=$CWD]" % sys.argv[0])
		quit()

	namespace = sys.argv[1].replace("-","_")
	resFolder = sys.argv[2]
	outFolder = sys.argv[3] if len(sys.argv) == 4 else None

	if not os.path.exists(resFolder):
		print("Folder '%s' does not exist" % resFolder)
		quit()

	if outFolder is not None and not os.path.exists(outFolder):
		print("Output folder '%s' does not exist" % outFolder)
		quit()

	# find resource files
	resFiles = []

	for root, dirs, files in os.walk(resFolder):
		for name in files:
			resFiles.append(os.path.abspath(os.path.join(root, name)))

	resFiles.sort()

	if outFolder is not None:
		os.chdir(outFolder)

	# create code now
	res2c(namespace, resFiles)

	sf = open("smartlist.h", "w")
	sf.write(smartlist)