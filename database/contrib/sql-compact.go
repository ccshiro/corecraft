package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"regexp"
)

var (
	FindInsert       = regexp.MustCompile("^INSERT INTO")
	ReplaceSemicolon = regexp.MustCompile(";\\r?\\n")
	ReplaceInsert    = regexp.MustCompile("^INSERT INTO.*?VALUES ")
)

func main() {
	bufin := bufio.NewReader(os.Stdin)
	bufout := bufio.NewWriter(os.Stdout)
	defer bufout.Flush()

	inserting := false

	for {
		// Read input
		line, err := bufin.ReadString('\n')
		switch {
		case err == io.EOF:
			return
		case err != nil:
			fmt.Fprintln(os.Stderr, "Error reading stdin:", err)
			return
		}
		
		// Do the compacting
		match := FindInsert.FindIndex([]byte(line))
		switch {
		case match != nil && inserting == false:
			// First insert, we replace the semicolon and the newline
			line = ReplaceSemicolon.ReplaceAllString(line, "")
			inserting = true
		case match != nil && inserting == true:
			// Subsequent insert, we output a comma and a newline and we replace the
			// INSERT INTO `table` VALUES, the semicolon and the newline
			bufout.WriteString(",\n")
			line = ReplaceInsert.ReplaceAllString(line, "\t")
			line = ReplaceSemicolon.ReplaceAllString(line, "")
		case match == nil && inserting == true:
			// No more inserts, write a semicolon and a newline
			bufout.WriteString(";\n")
			inserting = false
		}

		// Write output
		_, err = bufout.WriteString(line)
		if err != nil {
			fmt.Fprintln(os.Stderr, "Error writing to stdout:", err)
			return
		}
	}
}
