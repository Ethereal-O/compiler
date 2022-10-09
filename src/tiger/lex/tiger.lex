%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
 /* TODO: Put your lab2 code here */

%x COMMENT STR IGNORE

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* reserved words */
 /* TODO: Put your lab2 code here */
","    {adjust(); return Parser::COMMA;}
":="    {adjust(); return Parser::ASSIGN;}
":"    {adjust(); return Parser::COLON;}
";"    {adjust(); return Parser::SEMICOLON;}
"("    {adjust(); return Parser::LPAREN;}
")"    {adjust(); return Parser::RPAREN;}
"["    {adjust(); return Parser::LBRACK;}
"]"    {adjust(); return Parser::RBRACK;}
"{"    {adjust(); return Parser::LBRACE;}
"}"    {adjust(); return Parser::RBRACE;}
"."    {adjust(); return Parser::DOT;}
"+"    {adjust(); return Parser::PLUS;}
"-"    {adjust(); return Parser::MINUS;}
"*"    {adjust(); return Parser::TIMES;}
"/"    {adjust(); return Parser::DIVIDE;}
"="    {adjust(); return Parser::EQ;}
"<>"    {adjust(); return Parser::NEQ;}
"<="    {adjust(); return Parser::LE;}
"<"    {adjust(); return Parser::LT;}
">="    {adjust(); return Parser::GE;}
">"    {adjust(); return Parser::GT;}
"&"    {adjust(); return Parser::AND;}
"|"    {adjust(); return Parser::OR;}
"array" {adjust(); return Parser::ARRAY;}
"if"    {adjust(); return Parser::IF;}
"then"    {adjust(); return Parser::THEN;}
"else"    {adjust(); return Parser::ELSE;}
"while"    {adjust(); return Parser::WHILE;}
"for"    {adjust(); return Parser::FOR;}
"to"    {adjust(); return Parser::TO;}
"do"    {adjust(); return Parser::DO;}
"let"    {adjust(); return Parser::LET;}
"in"    {adjust(); return Parser::IN;}
"end"    {adjust(); return Parser::END;}
"of"    {adjust(); return Parser::OF;}
"break"    {adjust(); return Parser::BREAK;}
"nil"    {adjust(); return Parser::NIL;}
"function"    {adjust(); return Parser::FUNCTION;}
"var"    {adjust(); return Parser::VAR;}
"type"    {adjust(); return Parser::TYPE;}
[0-9]+    {adjust(); return Parser::INT;}
([a-z]|[A-Z]|"_")+([a-z]|[A-Z]|[1-9]|"_")*    {adjust(); return Parser::ID;}
 /* comment */
\/\*    {
            adjust();
            comment_level_ = 1;
            begin(StartCondition__::COMMENT);
        }
 /* error_comment */
<COMMENT><<EOF>>    {errormsg_->Error(errormsg_->tok_pos_, "unclosed comment");}
<COMMENT>    {
     /* recursive_of_comment */
    \/\*    {
                adjustStr();
                comment_level_++;
            }
     /* end_of_comment */
    \*\/    {
                adjustStr();
                if ((--comment_level_) == 0) {
                    begin(StartCondition__::INITIAL);
                }
            }
     /* content_of_comment */
    .|\n    {adjustStr();}
}
 /* string */
\"    {
          adjust();
          string_buf_.clear();
          begin(StartCondition__::STR);
      }
 /* error_string */
<STR><<EOF>>    {errormsg_->Error(errormsg_->tok_pos_, "unclosed string");}
<STR>    {
     /* end_of_string */
    \"    {
              adjustStr();
              setMatched(string_buf_);
              begin(StartCondition__::INITIAL);
              return Parser::STRING;
          }
     /* content_of_string */
    [^"\\]+    {adjustStr(); string_buf_ += matched();}
     /* change_line */
    \\[ \n\t]+\\    {adjustStr();}
     /* ascii_of_string */
    \\[0-9]{3}    {adjustStr(); string_buf_ += atoi(matched().substr(1).c_str());}
    \\\^[A-Z@\[\]\\\-6]    {
                      adjustStr();
                      char ch;
                      switch (matched().at(2)) {
                          case '@': ch = 0; break;
                          case '[': ch = 27; break;
                          case '\\': ch = 28; break;
                          case ']': ch = 29; break;
                          case '6': ch = 30; break;
                          case '-': ch = 31; break;
                          default: ch = matched().at(2) - 64;
                      }
                      string_buf_ += ch;
                  }
    \\.    {
                adjustStr();
                char ch;
                switch (matched().at(1)) {
                    case 'n': ch = '\n'; break;
                    case 't': ch = '\t'; break;
                    default: ch = matched().at(1);
                }
                string_buf_ += ch;
            }
}

 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
