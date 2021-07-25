(defun highlight-trace-file ()
  (interactive)
  (highlight-regexp "@0x[0-9a-f]\\{16\\}" 'hi-black-b)
  (highlight-regexp "V W" 'hi-red-b)
  (highlight-regexp "P C" 'hi-blue)
  (highlight-regexp "P W" 'hi-green-b)
  (highlight-regexp "0x[0-9a-f]\\{16\\}[\r\n]+" 'hi-yellow))

