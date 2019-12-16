all: index.html

index.html: src/Main.elm src/Controller.elm src/View.elm src/Model.elm
	elm make $< --output=$@

clean:
	rm -rf index.html
