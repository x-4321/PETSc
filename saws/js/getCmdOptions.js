//this function generates the command-line options for the given solver
//thus, passing in "0" (root solver) would generate the full command-line options
//important: use option='newline' to display each option on a new line. use option='space' to put spaces in between the options (as if we were using the terminal). option is space by default.
function getCmdOptions(endtag,prefix,option)
{
    if(prefix == undefined)
        prefix = "";

    var endl = "";
    if(option == "newline")
        endl = "<br>";
    else if(option == "space")
        endl = " ";
    else
        endl = " ";// use space by default

    var ret   = "";
    var index = getIndex(matInfo,endtag);
    if(index == -1)
        return "";

    ret += "-" + prefix + "pc_type " + matInfo[index].pc_type + endl;
    ret += "-" + prefix + "ksp_type " + matInfo[index].ksp_type + endl;

    var pc_type = matInfo[index].pc_type;

    if(pc_type == "mg") { //add extra info related to mg
        ret += "-" + prefix + "pc_mg_type " + matInfo[index].pc_mg_type + endl;
        ret += "-" + prefix + "pc_mg_levels " + matInfo[index].pc_mg_levels + endl;
    }
    else if(pc_type == "gamg") {
        ret += "-" + prefix + "pc_gamg_type " + matInfo[index].pc_gamg_type + endl;
        ret += "-" + prefix + "pc_gamg_levels " + matInfo[index].pc_gamg_levels + endl;
    }
    else if(pc_type == "fieldsplit") {
        ret += "-" + prefix + "pc_fieldsplit_type " + matInfo[index].pc_fieldsplit_type + endl;
        ret += "-" + prefix + "pc_fieldsplit_blocks " + matInfo[index].pc_fieldsplit_blocks + endl;
    }
    else if(pc_type == "bjacobi") {
        ret += "-" + prefix + "pc_bjacobi_blocks " + matInfo[index].pc_bjacobi_blocks + endl;
    }
    else if(pc_type == "asm") {
        ret += "-" + prefix + "pc_asm_blocks " + matInfo[index].pc_asm_blocks + endl;
        ret += "-" + prefix + "pc_asm_overlap " + matInfo[index].pc_asm_overlap + endl;
    }
    else if(pc_type == "redundant") {
        ret += "-" + prefix + "pc_redundant_number " + matInfo[index].pc_redundant_number + endl;
    }


    //then recursively handle all the children
    var numChildren = getNumChildren(matInfo,endtag);

    //handle children recursively
    for(var i=0; i<numChildren; i++) {
        var childEndtag = endtag + "_" + i;
        var childPrefix  = "";

        //first determine appropriate prefix
        if(pc_type == "mg")
            childPrefix = "mg_levels_" + i + "_";
        if(pc_type == "gamg")
            childPrefix = "gamg_levels_" + i + "_";
        else if(pc_type == "fieldsplit")
            childPrefix = "fieldsplit_" + i + "_";
        else if(pc_type == "bjacobi")
            childPrefix = "sub_";
        else if(pc_type == "asm")
            childPrefix = "sub_";
        else if(pc_type == "redundant")
            childPrefix = "redundant_";
        else if(pc_type == "ksp")
            childPrefix = "sub_";

        ret += getCmdOptions(childEndtag,prefix+childPrefix,option); //recursive call
    }

    return ret;
}

//this function is used by tree.js to get a simple description of a given solver
function getSimpleDescription(endtag)
{
    var ret  = "";
    var endl = "<br>";

    var index = getIndex(matInfo,endtag);
    if(index == -1)
        return "";

    ret += /*"ksp_type " +*/ matInfo[index].ksp_type + endl;
    ret += /*"pc_type " +*/ matInfo[index].pc_type + endl;

    /*var pc_type = matInfo[index].pc_type;

    if(pc_type == "mg") { //add extra info related to mg
        ret += "pc_mg_type " + matInfo[index].pc_mg_type + endl;
        ret += "pc_mg_levels " + matInfo[index].pc_mg_levels + endl;
    }
    else if(pc_type == "fieldsplit") {
        ret += "pc_fieldsplit_type " + matInfo[index].pc_fieldsplit_type + endl;
        ret += "pc_fieldsplit_blocks " + matInfo[index].pc_fieldsplit_blocks + endl;
    }
    else if(pc_type == "bjacobi") {
        ret += "pc_bjacobi_blocks " + matInfo[index].pc_bjacobi_blocks + endl;
    }
    else if(pc_type == "asm") {
        ret += "pc_asm_blocks " + matInfo[index].pc_asm_blocks + endl;
        ret += "pc_asm_overlap " + matInfo[index].pc_asm_overlap + endl;
    }
    else if(pc_type == "redundant") {
        ret += "pc_redundant_number " + matInfo[index].pc_redundant_number + endl;
    }*/

    return ret;
}
