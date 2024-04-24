# Who doesn't love to deal with escaping?!
function(shell_escape _str _out)
  string(REPLACE "'" "\\'" _str "${_str}")
  string(REPLACE "(" "\\(" _str "${_str}")
  string(REPLACE ")" "\\)" _str "${_str}")
  set("${_out}" "${_str}" PARENT_SCOPE)
endfunction()
