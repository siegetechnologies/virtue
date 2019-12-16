$(document).ready(function () {
	$("#numResults").change(function () {
		pag();//Redo pagination when results combobox changes value
	});
});	

/*Pag
	*Refresh data results,
	*quick way to force repagination and keep data fresh*/
function pag()
{
	include(window.state);
}

/*Paginate(page number)
	**/
function paginate(t)
{
	if(t <= 1) //if number of pages is <= 1, remove pagination
		{
			document.getElementById('pagi').innerHTML = '';
		}
	else //Draw new number on page
		{
			//Prev arrow and next arrow
	var pag = "<nav><ul class='pagination justify-content-center mt-2 mb-1 text-center'><li id='prev' class='page-item disabled'><a class='page-link' onClick='prevPage()'><i class='fas fa-arrow-left'></i></a></li>";
		var end = "<li id='next' class='page-item'><a class='page-link' onClick='nextPage()'><i class='fas fa-arrow-right'></i></a></li></ul></nav>";
		

		for (var n = 0; n < t; n++) //make all number and arrow buttons
		{
			var num = '0';
			if (n == 0) //make left arrow if first
			{
				 num = "<li id='pag-link" + (Number(n) + 1).toString() + "' style = 'width:2.7rem' class='page-item active'><a class='page-link' onClick='pageToggle(" + (Number(n) + 1).toString() + ")'>" + (Number(n) + 1).toString() + "</a></li>";
			} else //make number buttons
			{

				 num = "<li id='pag-link" + (Number(n) + 1).toString() + "' style = 'width:2.7rem' class='page-item'><a class='page-link'  onClick='pageToggle(" + (Number(n) + 1).toString() + ")'>" + (Number(n) + 1).toString() + "</a></li>";

				if(n == 1)//Add in hidded abbreviation icon
					{
						num += "<li id='abrevLow' style = 'width:2rem' class='page-item d-none'><a class='page-link disabled'><i class='fa fa-ellipsis-v'></i></a></li>";	
					}

				if (n == t-3) { //Add visible abbreviation icon
					num += "<li id='abrevHigh' style = 'width:2rem' class='page-item d-none'><a class='page-link disabled'><i class='fa fa-ellipsis-v'></i></a></li>";
				}

			}

			pag += num; //Attach new number button
		}
		pag += end; //Attach next arrow button


		document.getElementById('pagi').innerHTML = pag; //Attach pagination to the page

		var pagi = $('#pagi').children().children().children();
		var pagiSize = pagi.length;
		if (pagiSize >= 12) { //High number buttons if there are too many pages
			$('#abrevHigh').removeClass('d-none');
			for (var i = 8; i < pagiSize - 5; i++) {
				$('#pagi').children().children().children('#pag-link' + i).addClass('d-none');
			}
		}
		}
}

/*Page Toggle(pageNumber)
	*Jumps to a given page in data*/
function pageToggle(n) {

	checkPrev(n);//checks if first page, disables/enables prev arrow

	var results = $("#numResults").val();

	var x = n * results;

	checkNext(x);//checks if last page, disables/enables next arrow

	$('.page-item').removeClass('active');//remove highlighting
	$('#pag-link' + n).addClass('active');//add highlighting to active page
	$("#page").val(n);//log page number

	n--;//compensate for 0 start of array
	var rows = $( '#infoTable' ).children();
	rows.addClass('d-none');//hide all data
	var first = (n * results);//get index of first data
	var e = Number(first) + Number(results); //get index of end data
	if(rows.length < e)//check that end is not greater than total results
		{
			e = rows.length; //set end to size of data
		}
	for (var c = first; c < e; c++) {//show data on page
		if(rows[c].classList.contains("hidden"))//skip data filtered by search
		{
			e++;
		}
		else
		{
			rows[c].classList.remove("d-none");
		}
		
	}
	slideAbrev(); //Change abreviated numbers
	abrevCheck(); //check if abreviation is needed
}

/*Next Page
	*Brings user to the next page of data*/
function nextPage() {
	var n = $("#page").val();
	n = Number(n) + 1;
	pageToggle(n);
	slideAbrev();
	abrevCheck();
}

/*Previous Page
	*Brings user to the previous page of data*/
function prevPage() {
	var n = $("#page").val();
	n = Number(n) - 1;
	pageToggle(n);
	slideAbrev();
	abrevCheck();
}


/*Slide Abreviation
	*Change number buttons that are abreviated
	*Abreviation keeps page numbers from overflowing the page*/
function slideAbrev()
{
	
	var pag = $( '.pagination' ).children();
	var current = pag.index($( '.pagination' ).children('.active'));
	var size = pag.length;
	
	if(current > 5 && current < size -6)
				{
			for(var i = 0; i < size; i++)
				{
			
					if (i < 3 || i >= size -5)
						{
							
						}
					else if( i < current-3 )
						{
							$( '#pag-link'+i ).addClass( 'd-none' );
						}
					else if( i > current+1 )
						{
							$( '#pag-link'+i ).addClass( 'd-none' );
						}
					else
						{
							$( '#pag-link'+i ).removeClass( 'd-none' );
						}
				}
			
		}
	else if (current <= 5)
		{
			for(var x = 0; x < size - 5; x++)
				{
					if( x < 8 )
						{
							$( '#pag-link'+x ).removeClass( 'd-none' );
						}
					else
						{
							$( '#pag-link'+x ).addClass( 'd-none' );
						}
				}
		}
	else if (current > size - 5)
		{
			for(var c = 3; c < size ; c++)
				{
					if( c > size -10 )
						{
							$( '#pag-link'+c ).removeClass( 'd-none' );
						}
					else
						{
							$( '#pag-link'+c ).addClass( 'd-none' );
						}
				}
		}
}


/*Abreviation Check
	*Checks to see if high and low abreviation is nessesary*/
function abrevCheck()
{
	var pag = $( '.pagination' ).children();
	var h = pag.index($( '#abrevHigh' ));
	var offset = h - 2;
	if($( '#pag-link' + offset ).hasClass('d-none'))
		{
			$( '#abrevHigh' ).removeClass('d-none');
		}
		else
		{
			$( '#abrevHigh' ).addClass('d-none');
		}
	
		if($( '#pag-link3' ).hasClass('d-none'))
		{
			$( '#abrevLow' ).removeClass('d-none');
		}
		else
		{
			$( '#abrevLow' ).addClass('d-none');
		}
}

/*Check Previous
	*Checks to see if the page is the first
	*Disables or Enables the Previous page arrow button*/
function checkPrev(n) {

	if (n != 1) {
		$('#prev').removeClass("disabled");
	} else {
		$('#prev').addClass("disabled");
	}
}

/*Check Next
	*Checks to see if the page is the last
	*Disables or Enables the Next page arrow button*/
function checkNext(x) {
	var total = $("#infoTable").children('tr').length;
	if (x >= total) {
		$('#next').addClass("disabled");
	} else {
		$('#next').removeClass("disabled");
	}
}

/*Reset pagination
	*Sets page back to the first,
	*Changes abreviation, numbers and arrows to match the first page state*/
function resetPag()
{
	var n = 1;
	checkPrev(n);

	var results = $("#numResults").val();

	var x = n * results;

	checkNext(x);

	$('.page-item').removeClass('active');
	$('#pag-link' + n).addClass('active');
	$("#page").val(n);


	slideAbrev();
	abrevCheck();
}
