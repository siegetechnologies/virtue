$(document).ready(function () {

	$('#inputModal').modal({
		show: false
	})
	
	/*Launch Menu Change Index
		*Launch appropriate modal based off value,
		*Ensure globals variables are reset*/
	$('#MenuLaunch').on('change', function () {
		removeMultiSelect();
		switch (this.value) {
			case "CreateUser":
					window.assign1 = [];
					window.deassign1 = [];
					window.assign2 = [];
					window.deassign2 = [];
				window.action = 'create';
				CreateModal("createUser.html");
				break;
			case "CreateRole":
					window.assign1 = [];
					window.deassign1 = [];
					window.assign2 = [];
					window.deassign2 = [];
				window.action = 'create';
				CreateModal("createRole.html");
				break;
			case "CreateApp":
				window.action = 'create';
				CreateModal("createApplication.html");
				break;
			case "import":
				CreateModal("Import.html");
				window.action = 'import';
				break;

		}
	});

	/*Action Menu Change Index
		*Launch appropriate modal based off value,
		*Ensure globals variables are reset*/
	$('#MenuAction').on('change', function () {
		removeMultiSelect();
		switch (this.value) {
			case "DetailsVirtue":
				window.action = 'details';
				 CreateModal("detailsVirtue.html");
				break;
			case "DetailsUser":
					window.assign1 = [];
					window.deassign1 = [];

				window.action = 'details';
				CreateModal("detailsUser.html");
				break;
			case "DetailsRole":
					window.assign1 = [];
					window.deassign1 = [];

				window.action = 'details';
				CreateModal("detailsRole.html");
				break;
			case "DetailsApp":
				window.action = 'details';
				CreateModal("detailsApplication.html");
				break;
			case "delete":
				if(window.mode.value == 'Virtue')
					{
						CreateModal("confirmationV.html");
					}
				else
				{
					CreateModal("confirmation.html");
				}
				
				break;
		}
	});
	
	
	/*Enable Action Menu
		*Get selection on click (a bit slow since this runs on clicks anywhere on the screen, but reliable)
		* If selection exists enable action menu*/
		$(document).on('click', function() {
			var checked = false;
			
			if(window.state == 'App')
					{
						var boxes = $('#infoTable').children('.card').children().children('input');
					}
				else
					{
						var boxes = $('tbody').children('tr').children('td').children('input');
					}
			
			
			var size = boxes.length;
			for(var i = 0; i < size; i++)
				{
					if(boxes[i].checked)
						{
							checked = true;
							break;
						}
				}
			if(checked == true)
				{
					$('#MenuAction').prop('disabled', false);
					$('#MenuAction').addClass('btn-success');
					$('#MenuAction').removeClass('text-dark');
					$('#MenuAction').addClass('text-light');
				}
			else
				{
					$('#MenuAction').prop('disabled', true);
					$('#MenuAction').removeClass('btn-success');
					$('#MenuAction').removeClass('text-light');
					$('#MenuAction').addClass('text-dark');
				}
			
	});

});

function multiSelect() {
	$('#multiSubmit').removeClass('d-none');
	//$('.checkColumn').toggleClass('d-none');
}

function removeMultiSelect() {
	//$('.checkColumn').addClass('d-none');
	$('#multiSubmit').addClass('d-none');
	$('.checkColumn').prop('checked', 'false');
}


/*Create Modal
	*Fetches Modal html file
	*Url is set in a switch earlier in this file
	**/
function CreateModal(url) {
	$.ajax({
		async: true,
		method: "POST",
		cache: false,
		url: "modals/" + url,
		data: {},
		error: function (jqXHR, textStatus, errorThrown) {
			alert(errorThrown);
		},
		success: function (htmldata) {
			document.getElementById('inputModalContents').innerHTML = includeModalElements(htmldata);
			ModalReload();
		}
	});
	$('#inputModal').modal('show');
}

/*Close Modal Window*/
function CloseModal() {
	$('#inputModal').modal('hide');
}

/*ErrorModal, Closes Modal Window*/
function ErrorModal() {
	$('#inputModal').modal('hide');
}




/*Include Modal Elements
	*Strips Modal HTML file for the form
	*Returns String of HTML*/
function includeModalElements(data) {
	var start = data.indexOf("<body>") + 6;
	var end = data.lastIndexOf("</body>");
	return data.slice(start, end);
}