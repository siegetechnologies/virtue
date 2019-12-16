$(document).ready(function () {
	$("#filter").change(function () {
		filter($("#filter").val());//when user deselects search field, run search filter
	});

});


/*Filter
	*Hides rows of data that do not include the val string */
function filter(val) {
	var n = $("#numResults").val(); //get number of results perpage
	var count = 0;
	if (val == '') { //if empty string, show everything
		var rows = $('#infoTable').children();
		var count = rows.length //total number of objects to display
		for (var v = 0; v < count; v++) {
			if (v <= n) {
				rows[v].classList.remove('d-none'); //show everything
			}
			if (window.state != 'App') {
				rows[v].children[1].textContent = v + 1; //Renumber non app rows
			}
		}
		$('#infoTable').children().removeClass('hidden'); //remove hidden, only used for filtered rows.
		
	} else { //Valid string to use as filter
		$('#infoTable').children().addClass('hidden'); //Hide everything
		var cells = $('#infoTable').children().children(); //Array of cells, no reference to what row they are in
		var size = cells.length; //cell count
		count = 0;
		for (var i = 0; i < size; i++) {
			if (cells[i].textContent.toLowerCase().match(val.toLowerCase()) == val.toLowerCase()) { //Compare lower case version of strings.... Match returns val if val is found in the string. 
				//cells[i].parentElement.classList.remove('d-none');//Remove hidden status
				cells[i].parentElement.classList.remove('hidden'); //Remove hidden status
				if (window.state != 'App') {
					cells[i].parentElement.children[1].textContent = count + 1; //renumber non app rows
				}
				count++; //increase count, used to limit results per page
			} 
				
			}
		}
	
	var t = count / n;
	paginate(t); // Refresh pagination
}
