all: combined.pdf

combined.pdf: t1.rst cover.tmpl my.style ispras.png

%.pdf: %.rst
	rst2pdf -s kerning,colorful,my -c $<
