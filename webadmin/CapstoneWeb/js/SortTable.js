$(document).ready(function () {
	
	/*Trigger Sort Event
		*Get index of selected column
		*Pass index and mode to sortTable*/
	$('#data').on('click', '.header', function() {
		var index = $(this).index();
		switch($(this).children('i').hasClass('fa-caret-down'))
			{
				case true:
					$(this).children('i').removeClass('fa-caret-down').addClass('fa-caret-up');
					sortTable(index, 'Asc');
					break;
				case false:
					$(this).children('i').addClass('fa-caret-down').removeClass('fa-caret-up');
					sortTable(index, 'Dsc');
					break;
					
			}
	});

});


/*SortTable
	*Sort Table based off provided index and Mode*/
function sortTable(index, mode)
{
	//pag();
	var switching = true;
	$( '#infoTable' ).children().addClass('d-none');
	var rows = $( '#infoTable' ).children();
	var size = rows.length-1;
	while(switching)
		{
			switching = false;
			if(size >= 1)
				{
						for(var i = 0; i < size ; i++)
						{
							rows = $( '#infoTable' ).children();
							
							if(index == 0)//get row x and y Checked vs Unchecked
								{
									if(rows[i].children[index].children[0].checked)
										{
											var x = Number(1);
										}
										else
										{
											var x = Number(0);
										}
									if(rows[i+1].children[index].children[0].checked)
										{
											var y = Number(1);
										}
										else
										{
											var y = Number(0);
										}
								}
							else if (index == 1) //get row x and y
								{
									var x = Number(rows[i].children[index].textContent.toLowerCase());
									var y = Number(rows[i+1].children[index].textContent.toLowerCase());
								}
							else
								{
									var x = rows[i].children[index].textContent.toLowerCase();
									var y = rows[i+1].children[index].textContent.toLowerCase();
								}
							

							switch(mode) //Get sort
								{
									case 'Asc':
										if(x > y)
											{
												sort(rows[i],i);
												switching = true;
											}
										break;
									case 'Dsc':
										if(x < y)
											{
												sort(rows[i], i);
												switching = true;
											}
										break;
								}
						}
				}
		}
	
	rows = $( '#infoTable' ).children();
	var n = $("#numResults").val();
	for(var v = 0; v < n; v++)
		{
			if(rows[v])//Decides what is shown on a page
				{
						var shown = true;
						var list = rows[v].classList;
						var l = list.length;
					
						for(var z = 0; z < l; z++)
							{
								if(list[z] == "hidden")
									{
										shown = false;
									}
							}
						if(shown == true)
							{
								rows[v].classList.remove('d-none');
							}
						else
							{
								n++;
							}
				}
		
			
		}
	resetPag();//Restart pag at page 1
}


/*Sort
	*Merge sort*/
function sort(obj, index)
{
	var rows = $( '#infoTable' ).children();
	rows[index].parentNode.insertBefore(rows[index+1], rows[index]);
}
