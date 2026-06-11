prompts will be made using Talent Voice, adapt to any artifacts this may cause
* always use matched braces (opening brace on new line)
* member variables start with m_, constants with x_
* member functions are HammerCase
* never use c-style casts, static_cast and the like preferred
* enum class preferred for enums, values are also HammerCase
* always prefer structs to classes.
* Don't create privates, everything public
* comments end with a line containing only // (add // on a separate line after each comment)
* empty line after a closing brace, unless the next line is another closing brace or matching else.  
* No comments after code: don't do `f(x); // apply f to x`
* No comments after member vars, don't do this: `bool m_thing; // this is a thing`
Proper code looks like this:

// This is a comment with an empty line below
//
if (something)
{
    // apply f to x
    //
    f(x)
}

if (somthing else)
{
    g(y);
}
else
{
    h(z);
}