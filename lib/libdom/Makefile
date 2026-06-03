# (C) 2002-2026 The uBixOS Project
# libdom — NetSurf DOM library (vendored, MIT).  No build codegen (nsgenbind JS
# bindings are not used).  Includes the optional hubbub binding (HTML->DOM).

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
DOM      = ${SRCTOP}/contrib/libdom
HUB      = ${SRCTOP}/contrib/libhubbub
LPU      = ${SRCTOP}/contrib/libparserutils
LWC      = ${SRCTOP}/contrib/libwapcaplet

INCLUDES = -I${DOM}/include -I${DOM}/src -I${DOM}/bindings/hubbub \
           -I${HUB}/include -I${LPU}/include -I${LWC}/include \
           -I${MUSL_SRC}/include -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 -I${MUSL_SRC}/arch/generic

CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC -fcommon \
         -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP -Wall -O2

OBJDIR = ${OBJ_DIR}/obj/lib/libdom
OBJS   = attr.o cdatasection.o character_valid.o characterdata.o comment.o custom_event.o dispatch.o doc_fragment.o document_event.o document_type.o document.o element.o entity_ref.o event_listener.o event_target.o event.o hashtable.o html_anchor_element.o html_applet_element.o html_area_element.o html_base_element.o html_basefont_element.o html_body_element.o html_br_element.o html_button_element.o html_collection.o html_directory_element.o html_div_element.o html_dlist_element.o html_document.o html_element.o html_fieldset_element.o html_font_element.o html_form_element.o html_frame_element.o html_frameset_element.o html_head_element.o html_heading_element.o html_hr_element.o html_html_element.o html_iframe_element.o html_image_element.o html_input_element.o html_isindex_element.o html_label_element.o html_legend_element.o html_li_element.o html_link_element.o html_map_element.o html_menu_element.o html_meta_element.o html_mod_element.o html_object_element.o html_olist_element.o html_opt_group_element.o html_option_element.o html_options_collection.o html_paragraph_element.o html_param_element.o html_pre_element.o html_quote_element.o html_script_element.o html_select_element.o html_style_element.o html_table_element.o html_tablecaption_element.o html_tablecell_element.o html_tablecol_element.o html_tablerow_element.o html_tablesection_element.o html_text_area_element.o html_title_element.o html_ulist_element.o implementation.o keyboard_event.o mouse_event.o mouse_multi_wheel_event.o mouse_wheel_event.o mutation_event.o mutation_name_event.o namednodemap.o namespace.o node.o nodelist.o parser.o pi.o string.o text_event.o text.o typeinfo.o ui_event.o validate.o

.PATH: ${DOM}/src ${DOM}/src/core ${DOM}/src/utils ${DOM}/src/html ${DOM}/src/events ${DOM}/bindings/hubbub
.PATH.o: ${OBJDIR}
.SUFFIXES: .o .c
.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libdom.so
${OBJ_DIR}/lib/libdom.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m elf_i386 -shared -soname libdom.so ${OBJS:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lhubbub -lparserutils -lwapcaplet -lc -o ${OBJ_DIR}/lib/libdom.so

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libdom.so

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
