# Copyright (c) 2014 shiro <https://www.worldofcorecraft.com>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import urllib2
import re
import time

def fput(file, str):
    count = 0
    f = open(file, 'r')
    for l in f:
        count += 1
    f = open(file, 'a')
    f.write(str)
    return count + 1

def droppers(id):
    response = urllib2.urlopen('http://www.wowhead.com/item=' + str(id))
    html = response.read()

    lst = []

    found = False
    name = ""
    for item in html.split("\n"):
        if "<title>" in item:
            n = re.search(r'<title>(.+?) -', item)
            name = n.group(1)

        if "id: 'dropped-by'" in item:
            found = True
        if found and "data:" in item:

            drops = re.findall(r'"id":(.+?),.*?"name":"(.+?)",.+?count:(.+?),outof:(.+?)[,}]', item)
            for drop in drops:
                chance = 0
                if float(drop[3]) != 0:
                    chance = float(drop[2])/float(drop[3]) * 100 # * 100 because reasons
                lst.append((drop[1], drop[0], str(chance)))

            return name, lst
    return name, []

def mangos_query(id, name, lst):
    if len(lst) == 0:
        fput("failed", str(id) + "\n")
        return

    fput("query.sql", "-- " + name + " (" + str(id) + ")\n")
    for item in lst:
        fput("query.sql", "-- " + item[0] + "\n")
        ln = fput("query.sql", "INSERT INTO creature_loot_template (entry, item, ChanceOrQuestChance, minCountOrRef, maxcount) " +
            "VALUES(" + item[1] + ", " + str(id) + ", " + item[2] + ", 1, 1);\n")
        if float(item[2]) < 10:
            fput("warnings", "query.sql:" + str(ln) + ": chance lower than 10%\n")

# overwrite all output files
f = open("query.sql", "w")
f.close()
f = open("warnings", "w")
f.close()
f = open("failed", "w")
f.close()

f = open("input", "r")
for line in f:
    name, l = droppers(int(line))
    mangos_query(int(line), name, l)
    time.sleep(2)
