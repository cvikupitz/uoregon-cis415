import linkedlist as LL			# doubly-linked list implementation
import os				# access environment variables
import sys				# access arguments

directories = []			# empty list of directory strings

def open_file(afile):
    """
    Attempts to open `afile' in each directory of `directories'
    If successful, returns the file object
    If not, returns None
    """
    for d in directories:
        the_file = d + afile
        try:
            f = open(the_file, 'r')
            return f
        except:
            pass
    return None

def parse_file(afile):
    """
    Split filename in the form of root.ext into its constituent parts
    List is returned, with List[0] the root and List[1] the extension
    If no extension, returns afile as the root and an empty extension
    """
    j = afile.rfind('.')
    if j == -1:
        return [afile, '']
    else:
        return [afile[:j], afile[j+1:]]

def print_dependencies(the_table, printed, to_process):
    """
    iteratively print dependencies

    - use `printed' to guarantee that each file printed exactly once
    - use `to_process' to guarantee breadth-first order
    """
    fn = to_process.remove_first()
    while fn != None:
        ll = the_table[fn]
        for i in range(ll.length()):
            name = ll[i]
            if name not in printed:
                print(' {}'.format(name), end='')
                printed.add(name)
                to_process.add_last(name)
        fn = to_process.remove_first()
    return

def process(afile, deps, table, workq):
    """
    search `afile' for lines of the form #include "xxx.h"
    for each one found, append the filename to `deps'
    if the filename not found in `table', insert it with empty dependency list
        and append it to workq
    """
    f = open_file(afile)
    if f == None:
        print("Unable to open file:", afile)
        return
    for line in f:
        l = line.strip()			# remove whitespace
        if l.startswith('#include'):		# found #include line
            l = l[8:].lstrip()			# remove whitespace to filename
            if l[0] == '"':			# non-system include file
                li = []
                for i in range(1,len(l)):
                    if l[i] == '"':
                        break
                    li.append(l[i])
                fn = ''.join(li)		# create string from list
                deps.add_last(fn)		# add to dependencies for afile
                if fn not in table:
                    table[fn] = LL.LinkedList()	# add to table
                    workq.add_last(fn)		# add to work queue
    f.close()
    return

directories.append('./')		# always search current directory first
fstart = 0				# index into argv for first filename
for i in range(1,len(sys.argv)):
    if sys.argv[i].startswith("-I"):	# user specified another directory
        if sys.argv[i].endswith("/"):	# make sure directory ends in '/'
            directories.append(sys.argv[i])
        else:
            directories.append(sys.argv[i]+'/')
    else:				# all remaining arguments are filenames
        fstart = i
        break
cpath = os.environ.get('CPATH')		# obtain CPATH definition, if there
if cpath != None:
    dirs = cpath.split(':')		# break it into directories
    for i in range(len(dirs)):		# add to list of directories
        if dirs[i].endswith("/"):
            directories.append(dirs[i])
        else:
            directories.append(dirs[i]+'/')
#
# at this point, we have the list of directories to search for #include files
#
work_queue = LL.LinkedList()		# work queue of files to search
the_table = {}				# dictionary of files with dependencies
for i in range(fstart,len(sys.argv)):	# process each file
    L = parse_file(sys.argv[i])
    if L[1] != "c" and L[1] != "l" and L[1] != "y":
        sys.exit('Illegal argument: {} must end in .c, .l, or .y'.format(sys.argv[i]))
    obj = L[0] + ".o"
    deps = LL.LinkedList()
    deps.add_last(sys.argv[i])
    the_table[obj] = deps
    work_queue.add_last(sys.argv[i])
    deps = LL.LinkedList()
    the_table[sys.argv[i]] = deps
#
# at this point, we have all the .o's, .c's, .l's, and .y's in the_table
#
afile = work_queue.remove_first()
while afile != None:
    deps = the_table[afile]
    process(afile, deps, the_table, work_queue)
    the_table[afile] = deps
    afile = work_queue.remove_first()
#
# at this point, the table contains the full dependencies for all files.
#
for i in range(fstart,len(sys.argv)):	# process each file
    L = parse_file(sys.argv[i])
    obj = L[0] + ".o"
    print("{}:".format(obj), end='')
    printed = set()
    printed.add(obj)
    to_process = LL.LinkedList()
    to_process.add_last(obj)
    print_dependencies(the_table, printed, to_process)
    print()
sys.exit(0)
