/*
 * corpc-generator.cc
 *
 *  Created on: Sep 26, 2016
 *      Author: amyznikov
 */
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.pb.h>
#include <string>
#include <vector>
#include <stdlib.h>

using namespace std;
using namespace google;
using namespace google::protobuf;
using namespace google::protobuf::io;
using namespace google::protobuf::compiler;



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CF_Generator_Base {
  //GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CF_Generator_Base);

protected:

  typedef map<string,string>
    smap;

  typedef map<string,string>
    optsmap;

  optsmap opts;

protected:

  bool parse_opts(const string & text, string * status)
  {
    (void)(status);

    vector<string> parts;

    split(text, ",", &parts);

    for ( unsigned i = 0; i < parts.size(); i++ ) {
      const string::size_type equals_pos = parts[i].find_first_of('=');
      if ( equals_pos == string::npos ) {
        opts[parts[i]] = "y";
      }
      else {
        opts[parts[i].substr(0, equals_pos)] = parts[i].substr(equals_pos + 1);
      }
    }

    //    for (optsmap::const_iterator ii = opts->begin(); ii!= opts->end(); ++ii) {
    //      // check each option syntax here
    //    }

    return true;
  }









  template<typename _OI>
  static inline void split_to_iterator(const string & full, const char delim[], _OI & output)
  {
    // Optimize the common case where delim is a single character.
    if ( delim[0] != '\0' && delim[1] == '\0' ) {
      char c = delim[0];
      const char * p = full.data();
      const char * end = p + full.size();
      while ( p != end ) {
        if ( *p == c ) {
          ++p;
        }
        else {
          const char * start = p;
          while ( ++p != end && *p != c )
            ;
          *output++ = string(start, p - start);
        }
      }
    }
    else {
      string::size_type begin_index, end_index;
      begin_index = full.find_first_not_of(delim);
      while ( begin_index != string::npos ) {
        end_index = full.find_first_of(delim, begin_index);
        if ( end_index == string::npos ) {
          *output++ = full.substr(begin_index);
          return;
        }
        *output++ = full.substr(begin_index, (end_index - begin_index));
        begin_index = full.find_first_not_of(delim, end_index);
      }
    }
  }

  static void split(const string & full, const char* delim, vector<string>* result)
  {
    back_insert_iterator<vector<string> > output(*result);
    split_to_iterator(full, delim, output);
  }

  static bool has_suffix(const string & str, const string & suffix)
  {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  static string strip_suffix(const string& str, const string& suffix)
  {
    return has_suffix(str, suffix) ? str.substr(0, str.size() - suffix.size()) : str;
  }

  static string strip_proto_suffix(const string & fname)
  {
    return has_suffix(fname, ".protodevel") ? strip_suffix(fname, ".protodevel") : strip_suffix(fname, ".proto");
  }

  static string t2s(int x)
  {
    char s[16] = "";
    sprintf(s, "%2d", x);
    return s;
  }

  template<class T>
  static string name(const T * t)
  {
    const string name = t->name();
    vector<string> pieces;
    string rv = "";

    split(name, ".", &pieces);
    for ( size_t i = 0; i < pieces.size(); i++ ) {
      if ( !pieces[i].empty() ) {
        if ( !rv.empty() ) {
          rv += "_";
        }
        rv += pieces[i];
      }
    }
    return rv;
  }

  template<class T>
  string full_name(const T * t)
  {
    const string fullname = t->full_name();
    vector<string> pieces;
    string rv = "";

    split(fullname, ".", &pieces);
    for ( size_t i = 0, n = pieces.size(); i < n; ++i ) {
      if ( !pieces[i].empty() ) {
        if ( !rv.empty() ) {
          rv += "_";
        }
        rv += pieces[i];
      }
    }
    return rv;
  }


  // Convert a file name into a valid identifier.
  static string file_id(const string & filename)
  {
    string s;
    for ( size_t i = 0; i < filename.size(); i++ ) {
      s.push_back(isalnum(filename[i]) ? filename[i] : '_');
    }
    return s;
  }

  static string basename(const string & filename)
  {
    return strip_proto_suffix(filename);
  }

  static string basename(const FileDescriptor * file)
  {
    return strip_proto_suffix(file->name());
  }

  static string pb_h_filename(const string & basename)
  {
    return basename + ".pb.h";
  }

  static string pb_h_filename(const FileDescriptor * file)
  {
    return pb_h_filename(basename(file));
  }

  static string pb_c_filename(const string & basename)
  {
    return basename + ".pb.c";
  }

  static string pb_c_filename(const FileDescriptor * file)
  {
    return pb_c_filename(basename(file));
  }

  static string corpc_h_filename(const FileDescriptor * file)
  {
    return basename(file) + ".corpc.h";
  }

  static string corpc_c_filename(const FileDescriptor * file)
  {
    return basename(file) + ".corpc.c";
  }


  string field_name(const FieldDescriptor * field)
  {
    return name(field);
  }

  string enum_type_name(const EnumDescriptor * enum_type)
  {
    return full_name(enum_type);
  }

  string enum_member_name(const EnumValueDescriptor * enum_member)
  {
    return enum_member->type()->name() + "_" + enum_member->name();
  }

  string cfpbtype(const FieldDescriptor * field)
  {
    switch ( field->type() ) {
    case FieldDescriptor::TYPE_INT32    :    // int32, varint on the wire
      return "INT32   ";
    case FieldDescriptor::TYPE_SINT32   :    // int32, ZigZag-encoded varint on the wire
      return "INT32   ";
    case FieldDescriptor::TYPE_SFIXED32 :    // int32, exactly four bytes on the wire
      return "SFIXED32";
    case FieldDescriptor::TYPE_UINT32   :    // uint32, varint on the wire
      return "UINT32  ";
    case FieldDescriptor::TYPE_FIXED32  :    // uint32, exactly four bytes on the wire.
      return "FIXED32 ";

    case FieldDescriptor::TYPE_INT64    :    // int64, varint on the wire.
      return "INT64   ";
    case FieldDescriptor::TYPE_SINT64   :    // int64, ZigZag-encoded varint on the wire
      return "INT64   ";
    case FieldDescriptor::TYPE_SFIXED64 :    // int64, exactly eight bytes on the wire
      return "SFIXED64";
    case FieldDescriptor::TYPE_UINT64   :    // uint64, varint on the wire.
      return "UINT64  ";
    case FieldDescriptor::TYPE_FIXED64  :    // uint64, exactly eight bytes on the wire.
      return "FIXED64 ";

    case FieldDescriptor::TYPE_BOOL     :    // bool, varint on the wire.
      return "BOOL    ";

    case FieldDescriptor::TYPE_DOUBLE   :    // double, exactly eight bytes on the wire.
      return "DOUBLE  ";

    case FieldDescriptor::TYPE_FLOAT    :    // float, exactly four bytes on the wire.
      return "FLOAT   ";

    case FieldDescriptor::TYPE_ENUM     :    // Enum, varint on the wire
      return "ENUM    ";

    case FieldDescriptor::TYPE_STRING   :
      return "STRING  ";

    case FieldDescriptor::TYPE_BYTES    :
      return "BYTES   ";

    case FieldDescriptor::TYPE_GROUP :
    case FieldDescriptor::TYPE_MESSAGE :
      return "MESSAGE ";

    default:
        break;
    }

    return "BUG-HERE";
  }

  string cfctype(const FieldDescriptor * field)
  {
    switch ( field->type() ) {
      case FieldDescriptor::TYPE_DOUBLE :    // double, exactly eight bytes on the wire.
        return "double";
      case FieldDescriptor::TYPE_FLOAT :    // float, exactly four bytes on the wire.
        return "float";
      case FieldDescriptor::TYPE_INT32 :    // int32, varint on the wire
      case FieldDescriptor::TYPE_SFIXED32 :    // int32, exactly four bytes on the wire
      case FieldDescriptor::TYPE_SINT32 :    // int32, ZigZag-encoded varint on the wire
        return "int32_t";
      case FieldDescriptor::TYPE_UINT32 :    // uint32, varint on the wire
      case FieldDescriptor::TYPE_FIXED32 :    // uint32, exactly four bytes on the wire.
        return "uint32_t";
      case FieldDescriptor::TYPE_INT64 :    // int64, varint on the wire.
      case FieldDescriptor::TYPE_SFIXED64 :    // int64, exactly eight bytes on the wire
      case FieldDescriptor::TYPE_SINT64 :    // int64, ZigZag-encoded varint on the wire
        return "int64_t";
      case FieldDescriptor::TYPE_UINT64 :    // uint64, varint on the wire.
      case FieldDescriptor::TYPE_FIXED64 :    // uint64, exactly eight bytes on the wire.
        return "uint64_t";
      case FieldDescriptor::TYPE_BOOL :    // bool, varint on the wire.
        return "bool";
      case FieldDescriptor::TYPE_ENUM :    // Enum, varint on the wire
        return "enum " + full_name(field->enum_type());
      case FieldDescriptor::TYPE_STRING :    // String
        return "char *";
      case FieldDescriptor::TYPE_GROUP :      // Tag-delimited message.  Deprecated.
      case FieldDescriptor::TYPE_MESSAGE :    // Length-delimited message.
        return "struct " + full_name(field->message_type());
      case FieldDescriptor::TYPE_BYTES :    // Arbitrary byte array.
        return "uint8_t";

      default:
        break;
    }
    return "";
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CF_PBC_Generator
    : public CF_Generator_Base
{
  // GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CF_PBC_Generator);

public:
  CF_PBC_Generator()
  {
  }

  ~CF_PBC_Generator()
  {
  }

  bool Generate(const FileDescriptor * file, const string & opts, GeneratorContext * gctx, string * status)
  {
    if ( !parse_opts(opts, status) ) {
      return false;
    }


    { // Generate header.
      scoped_ptr<io::ZeroCopyOutputStream> output(gctx->Open(pb_h_filename(file)));
      Printer printer(output.get(), '$');
      generate_c_header(file, &printer);
    }

    { // Generate cc file.
      scoped_ptr<io::ZeroCopyOutputStream> output(gctx->Open(pb_c_filename(file)));
      Printer printer(output.get(), '$');
      generate_c_source(file, &printer);
    }

    return true;
  }




  void generate_c_header(const FileDescriptor * file, Printer * printer)
  {
    smap vars;

    vars["filename"] = file->name();
    vars["file_id"] = file_id(file->name());
    vars["c_filename"] = pb_c_filename(file);
    vars["h_filename"] = pb_h_filename(file);


    printer->Print(vars,
        "/*\n"
        " * $h_filename$\n"
        " *   Generated by the protocol buffer compiler from $filename$\n"
        " *   DO NOT EDIT!\n"
        " */\n"
        "#ifndef __$file_id$_pb_h__\n"
        "#define __$file_id$_pb_h__\n"
        "\n"
        "#include <cuttle/pb/pb.h>\n");


    for ( int i = 0; i < file->dependency_count(); i++ ) {
      vars["dep"] = pb_h_filename(file->dependency(i));
      printer->Print(vars, "#include \"$dep$\"\n");
    }

    printer->Print("\n\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n");


    if ( generate_enum_declarations(file, printer) ) {
      printer->Print("\n");
    }

    if ( generate_case_enums_for_unions(file, printer) ) {
      printer->Print("\n");
    }

    if ( file->message_type_count() > 0 ) {
      // Generate structs.
      generate_structs(file, printer);
      printer->Print("\n");
    }

    printer->Print(vars,
        "\n"
        "#ifdef __cplusplus\n"
        "} /* extern \"C\" */\n"
        "#endif\n"
        "\n\n#endif  /* __$file_id$_pb_h__ */\n");
  }


  void generate_c_source(const FileDescriptor * file, Printer * printer)
  {
    smap vars;

    vars["filename"] = file->name();
    vars["c_filename"] = pb_c_filename(file);
    vars["h_filename"] = pb_h_filename(file);

    // Generate top of header.
    printer->Print(vars,
        "/*\n"
        " * $c_filename$\n"
        " *   Generated by the protocol buffer compiler from $filename$\n"
        " *   DO NOT EDIT!\n"
        " */\n"
        "#include \"$h_filename$\"\n\n");

    if ( generate_enum_string_functions(file, printer) ) {
      printer->Print("\n");
    }

    if ( generate_case_enums_string_functions(file, printer) ) {
      printer->Print("\n");
    }

    if ( file->message_type_count() > 0 ) {
      generate_pb_fields(file, printer);
      printer->Print("\n");
    }
  }


  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Generate enum declarations


  int generate_enum_declarations(const FileDescriptor * file, Printer * printer)
  {
    map<string, string> vars;

    int enums_generated = 0;

    for ( int i = 0, n = file->enum_type_count(); i < n; ++i, ++enums_generated ) {

      const EnumDescriptor * enum_type = file->enum_type(i);
      const string enum_name = enum_type_name(enum_type);

      vars["enum_name"] = enum_name;

      printer->Print(vars,
          "typedef\n"
          "enum $enum_name$ {\n");

      printer->Indent();

      for ( int j = 0, m = enum_type->value_count(); j < m; ++j ) {
        const EnumValueDescriptor * enum_member = enum_type->value(j);
        printer->Print("$name$ = $value$,\n", "name", enum_member_name(enum_member), "value",
            t2s(enum_member->number()));
      }
      printer->Outdent();
      printer->Print(vars, "} $enum_name$;\n\n");

      printer->Print(vars,"const char * cf_$enum_name$_name(\n  enum $enum_name$ obj);\n\n");
    }

    return enums_generated;
  }

  int generate_enum_string_functions(const FileDescriptor * file, Printer * printer)
  {
    map<string, string> vars;
    int enums_generated = 0;

    for ( int i = 0, n = file->enum_type_count(); i < n; ++i, ++enums_generated ) {

      const EnumDescriptor * enum_type = file->enum_type(i);
      const string enum_name = enum_type_name(enum_type);

      vars["enum_name"] = enum_name;


      printer->Print(vars,"const char * cf_$enum_name$_name(enum $enum_name$ obj) {\n");
      printer->Indent();
      printer->Print("switch(obj) {\n");
      printer->Indent();

      for ( int j = 0, m = enum_type->value_count(); j < m; ++j ) {
        const EnumValueDescriptor * enum_member = enum_type->value(j);
        vars["member_name"] = enum_member_name(enum_member);
        vars["name"] = name(enum_member);
        printer->Print(vars, "case $member_name$: return \"$name$\";\n");
      }
      printer->Outdent();
      printer->Print("}\n");
      printer->Print(vars, "return \"unknown:$enum_name$\";\n");
      printer->Outdent();
      printer->Print("}\n\n");
    }

    return enums_generated;
  }


  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Generate the case enums for unions

  string oneof_enum_type_name(const OneofDescriptor * oneof)
  {
    return full_name(oneof) + "_tag";
  }

  string oneof_enum_member_name(const OneofDescriptor * oneof, const FieldDescriptor * field)
  {
    return full_name(oneof) + "_" + (field ? field_name(field) : string("none"));
  }

  int generate_case_enums_for_unions(const FileDescriptor * file, Printer * printer)
  {
    int enums_generated = 0;
    const Descriptor * type;
    const OneofDescriptor * oneof;
    const FieldDescriptor * field;

    for ( int j = 0, m = file->message_type_count(); j < m; ++j ) {

      type = file->message_type(j);

      for ( int i = 0, n = type->oneof_decl_count(); i < n; ++i, ++enums_generated ) {

        oneof = type->oneof_decl(i);
        const string enum_name = oneof_enum_type_name(oneof);

        printer->Print("typedef\n"
            "enum $enum_name$ {\n",
            "enum_name", enum_name);
        printer->Indent();

        printer->Print("$member_name$ = 0,\n", "member_name", oneof_enum_member_name(oneof, NULL));
        for ( int k = 0, l = oneof->field_count(); k < l; ++k ) {
          field = oneof->field(k);
          printer->Print("$name$ = $value$,\n", "name", oneof_enum_member_name(oneof, field), "value",
              t2s(field->number()));
        }

        printer->Outdent();
        printer->Print("} $enum_name$;\n\n", "enum_name", enum_name);

        printer->Print("const char * cf_$enum_name$_name(\n"
            "  enum $enum_name$ tag);\n\n",
            "enum_name", enum_name);
      }
    }
    return enums_generated;
  }

  int generate_case_enums_string_functions(const FileDescriptor * file, Printer * printer)
  {
    int enums_generated = 0;
    const Descriptor * type;
    const OneofDescriptor * oneof;
    const FieldDescriptor * field;
    map<string, string> vars;

    for ( int j = 0, m = file->message_type_count(); j < m; ++j ) {

      type = file->message_type(j);

      for ( int i = 0, n = type->oneof_decl_count(); i < n; ++i, ++enums_generated ) {


        oneof = type->oneof_decl(i);
        const string enum_name = oneof_enum_type_name(oneof);

        vars["enum_name"] = enum_name;

        printer->Print(vars, "const char * cf_$enum_name$_name(enum $enum_name$ tag) {\n");
        printer->Indent();
        printer->Print("switch( tag ) {\n");
        printer->Indent();

        vars["member_name"] = oneof_enum_member_name(oneof, NULL);
        printer->Print(vars, "case $member_name$: return \"nothing\";\n");

        for ( int k = 0, l = oneof->field_count(); k < l; ++k ) {
          field = oneof->field(k);
          vars["member_name"] = oneof_enum_member_name(oneof, field);
          vars["name"] = field->name();
          printer->Print(vars, "case $member_name$: return \"$name$\";\n");
        }

        printer->Outdent();
        printer->Print("}\n");
        printer->Print(vars, "return \"unknown:$enum_name$\";\n");
        printer->Outdent();
        printer->Print("}\n\n");
      }
    }
    return enums_generated;
  }


  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Generate structs

  void generate_structs(const FileDescriptor * file, Printer * printer)
  {
    for ( int i = 0, n = file->message_type_count(); i < n; ++i ) {
      generate_struct(file->message_type(i), printer);
      printer->Print("\n");
    }
  }

  void generate_struct(const Descriptor * type, Printer * printer)
  {
    map<string, string> vars;
    const FieldDescriptor * field;
    const OneofDescriptor * oneof;

    for ( int i = 0, n = type->nested_type_count(); i < n; ++i ) {
      generate_struct(type->nested_type(i), printer);
      printer->Print("\n");
    }

    vars["class_name"] = full_name(type);

    // Generate struct fields
    printer->Print(vars,
        "typedef\n"
        "struct $class_name$ {\n");
    printer->Indent();

    for ( int i = 0, n = type->field_count(); i < n; ++i ) {
      if ( !(field = type->field(i))->containing_oneof() ) {
        generate_struct_member(field, printer);
      }
    }

    // Generate unions from oneofs.
    for ( int j = 0, m = type->oneof_decl_count(); j < m; ++j ) {

      oneof = type->oneof_decl(j);
      const string enum_name = oneof_enum_type_name(oneof);

      printer->Print("\nstruct {\n");
      printer->Indent();
      printer->Print("uint32_t tag; /* enum $enum_name$ */\n", "enum_name", enum_name);
      printer->Print("union {\n");
      printer->Indent();
      for ( int k = 0, l = oneof->field_count(); k < l; ++k ) {
        generate_struct_member(oneof->field(k), printer);
      }
      printer->Outdent();
      printer->Print("};\n");
      printer->Outdent();
      printer->Print("} $name$;\n\n", "name", name(oneof));
    }


    for ( int i = 0, n = type->field_count(); i < n; ++i ) {
      if ( !(field = type->field(i))->containing_oneof() ) {
        generate_struct_has_member(field, printer);
      }
    }


    printer->Outdent();
    printer->Print(vars, "} $class_name$;\n\n");
    printer->Print(vars, "extern const cf_pb_field_t $class_name$_fields[];\n\n");


//    printer->Print(vars,
//        "bool cf_$class_name$_init(struct $class_name$ * obj, const struct $class_name$_init_args * args);\n");
//    printer->Print(vars,
//        "void cf_$class_name$_cleanup(struct $class_name$ * obj);\n\n");

    printer->Print(vars,
        "size_t cf_pb_pack_$class_name$(const struct $class_name$ * obj,void ** buf);\n");
    printer->Print(vars,
        "bool cf_pb_unpack_$class_name$(struct $class_name$ * obj,const void * buf, size_t size);\n\n");


    printer->Print("\n\n");

  }


  void generate_struct_member(const FieldDescriptor * field, Printer * printer)
  {
    map<string, string> vars;

    vars["field_name"] = field_name(field);
    vars["field_type"] = cfctype(field);

    if ( field->label() == FieldDescriptor::LABEL_REPEATED ) {
      printer->Print(vars, "ccarray_t $field_name$; /* <$field_type$> */\n");
    }
    else if ( field->type() == FieldDescriptor::TYPE_BYTES ) {
      printer->Print(vars, "cf_membuf $field_name$; /* <$field_type$> */\n");
    }
    else {
      printer->Print(vars, "$field_type$ $field_name$;\n");
    }
  }

  void generate_struct_has_member(const FieldDescriptor * field, Printer * printer)
  {
    if ( field->label() == FieldDescriptor::LABEL_OPTIONAL && !field->containing_oneof() ) {
      printer->Print("bool has_$name$;\n", "name", field_name(field));
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  void generate_pb_fields(const FileDescriptor * file, Printer * printer)
  {
    for ( int i = 0, n = file->message_type_count(); i < n; ++i ) {
      generate_pb_fields_for_struct(file->message_type(i), printer);
      printer->Print("\n");
    }
  }

  void generate_pb_fields_for_struct(const Descriptor * type, Printer * printer)
  {
    map<string, string> vars;
    const FieldDescriptor * field;
    int i, n;

    for ( i = 0, n = type->nested_type_count(); i < n; ++i ) {
      generate_pb_fields_for_struct(type->nested_type(i), printer);
      printer->Print("\n");
    }

    vars["class_name"] = full_name(type);

    printer->Print(vars, "const cf_pb_field_t $class_name$_fields[] = {\n");
    printer->Indent();

    for ( i = 0, n = type->field_count(); i < n; ++i ) {
      field = type->field(i);

      vars["tag"] = t2s(field->number());
      vars["name"] = field_name(field);
      vars["pbtype"] = cfpbtype(field);
      vars["ctype"] = cfctype(field);
      vars["ptr"] = cfdescptr(field);

      if ( field->containing_oneof() ) {
        // CF_PB_ONEOF_FIELD
        vars["oneof"] = name(field->containing_oneof());
        printer->Print(vars,
            "CF_PB_ONEOF_FIELD   ($class_name$,\t$tag$,\tCF_PB_$pbtype$,\t$oneof$,\t$name$,\t$ctype$,\t$ptr$),\n");
      }
      else if ( field->label() == FieldDescriptor::LABEL_REPEATED ) {
        printer->Print(vars,
            "CF_PB_REQUIRED_FIELD($class_name$,\t$tag$,\tCF_PB_$pbtype$,\tCF_PB_ARRAY ,\t$name$,\t$ctype$,\t$ptr$),\n");
      }
      else if ( field->label() == FieldDescriptor::LABEL_REQUIRED ) {
        printer->Print(vars,
            "CF_PB_REQUIRED_FIELD($class_name$,\t$tag$,\tCF_PB_$pbtype$,\tCF_PB_SCALAR, $name$, $ctype$, $ptr$),\n");
      }
      else {
        printer->Print(vars,
            "CF_PB_OPTIONAL_FIELD($class_name$,\t$tag$,\tCF_PB_$pbtype$,\tCF_PB_SCALAR, $name$, $ctype$, $ptr$),\n");
      }
    }

    printer->Print("CF_PB_LAST_FIELD\n");
    printer->Outdent();
    printer->Print(vars, "};\n\n");


    printer->Print(vars,
        "size_t cf_pb_pack_$class_name$(const struct $class_name$ * obj, void ** buf) {\n"
        "  return cf_pb_pack(obj, $class_name$_fields, buf);\n"
        "}\n\n");

    printer->Print(vars,
        "bool cf_pb_unpack_$class_name$(struct $class_name$ * obj, const void * buf, size_t size) {\n"
        "  return cf_pb_unpack(buf, size, $class_name$_fields, obj);\n"
        "}\n\n\n");

  }



  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




  string cfdescptr(const FieldDescriptor * field)
  {
    switch ( field->type() ) {
    case FieldDescriptor::TYPE_GROUP :
    case FieldDescriptor::TYPE_MESSAGE :
      return full_name(field->message_type()) + "_fields";
    default:
      break;
    }
    return "NULL";
  }




  //////////////////////////

};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CF_CORPC_Generator
    : public CF_Generator_Base
{
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CF_CORPC_Generator);

public:
  CF_CORPC_Generator()
  {
  }

  ~CF_CORPC_Generator()
  {
  }

  bool Generate(const FileDescriptor * file, const string & opts, GeneratorContext * gctx, string * status)
  {
    if ( !parse_opts(opts, status) ) {
      return false;
    }

    { // Generate header.
      scoped_ptr<io::ZeroCopyOutputStream> output(gctx->Open(corpc_h_filename(file)));
      Printer printer(output.get(), '$');
      generate_c_header(file, &printer);
    }


    { // Generate c file.
      scoped_ptr<io::ZeroCopyOutputStream> output(gctx->Open(corpc_c_filename(file)));
      Printer printer(output.get(), '$');
      generate_c_source(file, &printer);
    }

    return true;
  }

  void generate_c_header(const FileDescriptor * file, Printer * printer)
  {
    smap vars;

    vars["filename"] = file->name();
    vars["file_id"] = file_id(file->name());
    vars["c_filename"] = corpc_c_filename(file);
    vars["h_filename"] = corpc_h_filename(file);
    vars["pb_h_filename"] = pb_h_filename(file);


    printer->Print(vars,
        "/*\n"
        " * $h_filename$\n"
        " *   Generated by the protocol buffer compiler from $filename$\n"
        " *   DO NOT EDIT!\n"
        " */\n"
        "#ifndef __$file_id$_corpc_h__\n"
        "#define __$file_id$_corpc_h__\n"
        "\n"
        "#include \"$pb_h_filename$\"\n"
        "#include <cuttle/corpc/channel.h>\n"
        );


    printer->Print("\n\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n");


    generate_sendrecv_declarations(file, printer);


    printer->Print(vars,
        "\n"
        "#ifdef __cplusplus\n"
        "} /* extern \"C\" */\n"
        "#endif\n"
        "\n\n#endif  /* __$file_id$_pb_h__ */\n");
  }




  void generate_c_source(const FileDescriptor * file, Printer * printer)
  {
    smap vars;

    vars["filename"] = file->name();
    vars["c_filename"] = corpc_c_filename(file);
    vars["h_filename"] = corpc_h_filename(file);

    // Generate top of header.
    printer->Print(vars,
        "/*\n"
        " * $c_filename$\n"
        " *   Generated by the protocol buffer compiler from $filename$\n"
        " *   DO NOT EDIT!\n"
        " */\n"
        "#include \"$h_filename$\"\n\n"
        );


    generate_sendrecv_definitions(file, printer);
  }








  void generate_sendrecv_declarations(const FileDescriptor * file, Printer * printer)
  {
    for ( int i = 0, n = file->message_type_count(); i < n; ++i ) {
      generate_sendrecv_declaration(file->message_type(i), printer);
      printer->Print("\n");
    }
  }

  void generate_sendrecv_declaration(const Descriptor * type, Printer * printer)
  {
    smap vars;

    for ( int i = 0, n = type->nested_type_count(); i < n; ++i ) {
      generate_sendrecv_declaration(type->nested_type(i), printer);
      printer->Print("\n");
    }

    vars["type"] = full_name(type);

    printer->Print(vars,
        "bool corpc_stream_write_$type$(corpc_stream * st, const struct $type$ * obj);\n"
        "bool corpc_stream_read_$type$(corpc_stream * st, struct $type$ * obj);\n\n"
        );
  }


  void generate_sendrecv_definitions(const FileDescriptor * file, Printer * printer)
  {
    for ( int i = 0, n = file->message_type_count(); i < n; ++i ) {
      generate_sendrecv_definition(file->message_type(i), printer);
      printer->Print("\n");
    }
  }


  void generate_sendrecv_definition(const Descriptor * type, Printer * printer)
  {
    smap vars;

    for ( int i = 0, n = type->nested_type_count(); i < n; ++i ) {
      generate_sendrecv_definition(type->nested_type(i), printer);
      printer->Print("\n");
    }

    vars["type"] = full_name(type);

    printer->Print(vars,
        "static size_t corpc_pack_$type$(const void * obj, void ** buf)\n"
        "{\n"
        "  return cf_pb_pack_$type$(obj, buf);\n"
        "}\n"
        "\n"
        "bool corpc_stream_write_$type$(corpc_stream * st, const struct $type$ * obj)\n"
        "{\n"
        "  return corpc_stream_write_msg(st, corpc_pack_$type$, obj);\n"
        "}\n"
        "\n"
        "static bool corpc_unpack_$type$(void * obj, const void * buf, size_t size)\n"
        "{\n"
        "  return cf_pb_unpack_$type$(obj, buf, size);\n"
        "}\n"
        "\n"
        "bool corpc_stream_read_$type$(corpc_stream * st, struct $type$ * obj)\n"
        "{\n"
        "  return corpc_stream_read_msg(st, corpc_unpack_$type$, obj);\n"
        "}\n"
        "\n"
        "\n"
        );
  }
  /*
   *         "bool cf_pb_unpack_$class_name$(struct $class_name$ * obj, const void * buf, size_t size) {\n"
        "  return cf_pb_unpack(buf, size, $class_name$_fields, obj);\n"
        "}\n\n\n");
   *
   */

};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template<class G>
class Generator
      : public CodeGenerator
{
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Generator);
  G * generator;
public:
  Generator() {
    generator = new G();
  }

  ~Generator() {
    delete generator;
  }

  bool Generate(const FileDescriptor * file, const string & options, GeneratorContext * gctx, string * status) const {
    return generator->Generate(file, options, gctx, status);
  }
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
int main(int argc, char *argv[])
{
  CommandLineInterface cli;
  Generator<CF_PBC_Generator> g1;
  Generator<CF_CORPC_Generator> g2;

  cli.RegisterGenerator("--c_out", "--c_opt", &g1, "Generate C/H files.");
  cli.RegisterGenerator("--corpc_out", "--corpc_opt", &g2, "Generate CORPC C/H files.");

  // Add version info generated by automake
  // cli.SetVersionInfo(PACKAGE_STRING);

  return cli.Run(argc, argv);
}




